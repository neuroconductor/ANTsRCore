// ===========================================================================
//
//                            PUBLIC DOMAIN NOTICE
//            Office of High Performance Computing and Communications
//
//  This software/database is a "United States Government Work" under the
//  terms of the United States Copyright Act.  It was written as part of
//  the author's official duties as a United States Government employee and
//  thus cannot be copyrighted.  This software is freely available
//  to the public for use. The National Library of Medicine and the U.S.
//  Government have not placed any restriction on its use or reproduction.
//
//  Although all reasonable efforts have been taken to ensure the accuracy
//  and reliability of the software and data, the NLM and the U.S.
//  Government do not and cannot warrant the performance or results that
//  may be obtained by using this software or data. The NLM and the U.S.
//  Government disclaim all warranties, express or implied, including
//  warranties of performance, merchantability or fitness for any particular
//  purpose.
//
//  Please cite the author in any work or product based on this material.
//
// ===========================================================================
//

#ifndef itkMultiScaleLaplacianBlobDetectorImageFilter2_hxx
#define itkMultiScaleLaplacianBlobDetectorImageFilter2_hxx

#include "itkMultiScaleLaplacianBlobDetectorImageFilter2.h"
#include "itkEllipseSpatialObject.h"
#include "itkCastImageFilter.h"
#include "itkLaplacianRecursiveGaussianImageFilter.h"
#include "itkNeighborhoodAlgorithm.h"
#include "itkProgressAccumulator.h"

#include <iterator>

namespace itk
{


template < class TInputImage >
MultiScaleLaplacianBlobDetectorImageFilter2< TInputImage >
::MultiScaleLaplacianBlobDetectorImageFilter2( void )
{
  m_NumberOfBlobs = 1000;
  m_StepsPerOctave = 15;
  m_StartT = 8;
  m_EndT = 128;
}

template < class TInputImage >
void MultiScaleLaplacianBlobDetectorImageFilter2< TInputImage >
::GenerateData( void )
{
  itkDebugMacro(<< " generating data ");


  const InputImageConstPointer inputImage( this->GetInput() );

  const InputImagePointer outputImage( this->GetOutput() );

  typedef itk::CastImageFilter<InputImageType, InputImageType> CasterFilterType;
  typename CasterFilterType::Pointer caster = CasterFilterType::New();
  caster->SetNumberOfWorkUnits( this->GetNumberOfWorkUnits() );
  caster->InPlaceOff();
  caster->SetInput( inputImage );

  caster->GraftOutput( outputImage );
  caster->Update();
  this->GraftOutput( caster->GetOutput() );

  // Create a process accumulator for tracking the progress of minipipeline
  ProgressAccumulator::Pointer progress = ProgressAccumulator::New();
  progress->SetMiniPipelineFilter(this);

  // prepare one time threaded data
  this->m_BlobHeapPerThread.resize( this->GetNumberOfWorkUnits() );


  // we wish to add an additional laplacian before and after the user
  // defined range to check for maximums
  const double k = std::pow( 2.0, 1.0 / m_StepsPerOctave );
  const double initial_sigma = std::sqrt(m_StartT) * 1.0/k;

  const unsigned int numberOfScales = std::ceil( std::log( std::sqrt( m_EndT ) / initial_sigma ) / std::log( k ) ) + 1.0;

  typedef itk::LaplacianRecursiveGaussianImageFilter< InputImageType, RealImageType> LaplacianFilterType;
  typename LaplacianFilterType::Pointer laplacianFilter[3];

  for (unsigned int i = 0; i < 3; ++i )
    {
    laplacianFilter[i] = LaplacianFilterType::New();
    laplacianFilter[i]->SetNumberOfWorkUnits( this->GetNumberOfWorkUnits() );
    laplacianFilter[i]->SetInput( inputImage );
    laplacianFilter[i]->SetNormalizeAcrossScale( true );
    progress->RegisterInternalFilter( laplacianFilter[i],   1.0 / numberOfScales );
    }

  BlobHeapType blobs;
  m_GlobalMinimalBestBlobValue = itk::NumericTraits<RealPixelType>::NonpositiveMin();

  for ( unsigned int i = 0; i < numberOfScales; ++i )
    {
    // simga' = k^i * initial_sigma
    // t = sigma^2
    const double sigma = initial_sigma * std::pow( k, double( numberOfScales - i -1 ) );

#if !defined( NDEBUG )
    const double iterT = vnl_math::sqr( initial_sigma * std::pow( k, double( numberOfScales - i - 1 ) ) );
#endif

    itkDebugMacro( << "i: " << i << " sigma: " << sigma << " t: " << iterT << " k: " << k );

    // update largest index with new largest sigma
    laplacianFilter[2]->SetSigma( sigma );

    laplacianFilter[2]->Update();

    // wait until all three laplacian filters have run
    if ( i > 1 )
      {
      // prepare for threaded execution
      this->m_LaplacianImage[0] = laplacianFilter[0]->GetOutput();
      this->m_LaplacianImage[1] = laplacianFilter[1]->GetOutput();
      this->m_LaplacianImage[2] = laplacianFilter[2]->GetOutput();
      this->m_CurrentProgress = this->GetProgress();

      // Set up the multithreaded processing of the outputs
      typename Superclass::ThreadStruct str;
      str.Filter = this;

      this->GetMultiThreader()->SetNumberOfWorkUnits( this->GetNumberOfWorkUnits() );
      this->GetMultiThreader()->SetSingleMethod( Self::ThreaderCallback, &str );

      // multithread the execution
      this->GetMultiThreader()->SingleMethodExecute();

      // combine all the maximally sorted lists into a maximally
      // sorted one
      for ( unsigned int t = 0; t < m_BlobHeapPerThread.size(); ++t )
        {

        // after threaded execution the heap will be sorted
        BlobHeapType &threadBlobs = m_BlobHeapPerThread[t];

        if ( threadBlobs.empty() )
          {
          continue;
          }

        BlobHeapType temp;
        swap(temp, blobs);
        blobs.resize( threadBlobs.size() + temp.size() );

        std::merge( temp.begin(), temp.end(),
                    threadBlobs.begin(), threadBlobs.end(),
                    blobs.begin(), BlobValueGreaterCompare );


        if ( blobs.size() > m_NumberOfBlobs )
          {
          blobs.resize( m_NumberOfBlobs );
          }

        threadBlobs.clear();

        }

      // set the minimal value
      if ( !blobs.empty() )
        {
        m_GlobalMinimalBestBlobValue = blobs.back().m_Value;
        }
      }

    // circularly rotate laplacian filters down in scale
    swap( laplacianFilter[2], laplacianFilter[1] );
    swap( laplacianFilter[2], laplacianFilter[0] );
    // Current Sigma refers to the center filter, hence next it'll be
    // the center
    m_CurrentSigma = sigma;

    // after each pass reset progress to accumulate next iteration
    progress->ResetFilterProgressAndKeepAccumulatedProgress();
    }

  // clean up member variables
  this->m_LaplacianImage[0] = NULL;
  this->m_LaplacianImage[1] = NULL;
  this->m_LaplacianImage[2] = NULL;
  this->m_BlobHeapPerThread.clear();

  m_BlobList.clear();
  //typename InputImageType::SpacingType spacing = inputImage->GetSpacing();

  typename BlobType::PointType zeroPoint;
  zeroPoint.Fill(0);

  // Convert to SpatialObject blob
  for ( typename BlobHeapType::const_iterator i = blobs.begin(); i != blobs.end(); ++i )
    {
    itkDebugMacro( "Blob detected at " << i->m_Center  << " with value " << i->m_Value << " and sigma: " << i->m_Sigma );

    const double sigma =  std::sqrt( InputImageType::ImageDimension / 2.0 )* i->m_Sigma;

    BlobPointer blob = BlobType::New();
    blob->SetSigmaInObjectSpace( sigma );
    blob->SetScaleSpaceValue( i->m_Value );
    blob->SetCenter(  i->m_Center );

    // transform the center index into offset vector
    typename BlobType::PointType centerPoint;
    inputImage->TransformIndexToPhysicalPoint( i->m_Center, centerPoint );

    const typename BlobType::VectorType centerVector = centerPoint - zeroPoint;

    blob->GetObjectToParentTransform()->SetOffset(centerVector);
//    blob->ComputeBoundingBox();

    m_BlobList.push_back( blob );
    }

}


template < class TInputImage >
void MultiScaleLaplacianBlobDetectorImageFilter2< TInputImage >
::ThreadedGenerateData(const OutputImageRegionType &outputRegionForThread, ThreadIdType threadId )
{
  //unsigned int m_Number = 20;
  //  const unsigned int numberOfScales = m_Number+2;
//     const unsigned int numberOfFilters = numberOfScales + m_Number;

//     ProgressReporter progress( this, threadId, outputRegionForThread.GetNumberOfPixels(), 100, m_CurrentProgress,  1.0/numberOfFilters);

  BlobHeapType &blobHeap = this->m_BlobHeapPerThread[threadId];
  blobHeap.reserve( m_NumberOfBlobs );

  RealPixelType localMinimalBestBlobValue = m_GlobalMinimalBestBlobValue;

  // center laplacian image
  typename RealImageType::ConstPointer laplacianImage =  this->m_LaplacianImage[1];
  InputImagePointer outputImage( this->GetOutput() );


  typedef itk::ConstNeighborhoodIterator< RealImageType > NeighborhoodIteratorType;
  typename NeighborhoodIteratorType::RadiusType radius;
  radius.Fill(1);


  typedef itk::NeighborhoodAlgorithm::ImageBoundaryFacesCalculator< RealImageType > FaceCalculatorType;
  FaceCalculatorType faceCalculator;
  typename FaceCalculatorType::FaceListType faceList;
  typename FaceCalculatorType::FaceListType::iterator fit;

  faceList = faceCalculator( laplacianImage, outputRegionForThread, radius );


  Point<double, RealImageType::ImageDimension> zeroPoint;
  zeroPoint.Fill(0);


  for ( fit = faceList.begin(); fit != faceList.end(); ++fit )
    {
    NeighborhoodIteratorType n0( radius, this->m_LaplacianImage[0], *fit );
    NeighborhoodIteratorType n1( radius, this->m_LaplacianImage[1], *fit );
    NeighborhoodIteratorType n2( radius, this->m_LaplacianImage[2], *fit );

    const size_t neighborhoodSize = n0.Size();


    while ( !n0.IsAtEnd() && !n1.IsAtEnd() && !n2.IsAtEnd() )
      {
      const RealPixelType center = n1.GetCenterPixel();
      bool isUsable = true;

      if( center <  localMinimalBestBlobValue )
        {
        isUsable = false;
        }
      else
        {
        for ( size_t h = 0; h < neighborhoodSize && isUsable; ++h )
          {
          if ( center <  n0.GetPixel( h ) ) isUsable = false;
          }
        for ( size_t h = 0; h < neighborhoodSize && isUsable; ++h )
          {
          if ( center <  n1.GetPixel( h ) ) isUsable = false;
          }
        for ( size_t h = 0; h < neighborhoodSize && isUsable; ++h )
          {
          if ( center <  n2.GetPixel( h ) ) isUsable = false;
          }
        }

      if ( isUsable )
        {

        // add blob to thread list

        Blob blob(  n1.GetIndex(), m_CurrentSigma, center );


        // maintain a minimum heap ( first element is less than all
        // others) no greater then the target number of blobs
        if ( blobHeap.size() < m_NumberOfBlobs )
          {
          blobHeap.push_back( blob );
          std::push_heap( blobHeap.begin(), blobHeap.end(), BlobValueGreaterCompare );
          }
        else if (blob.m_Value > localMinimalBestBlobValue )
          {
          std::pop_heap( blobHeap.begin(), blobHeap.end(), BlobValueGreaterCompare );
          blobHeap.back() = blob;
          std::push_heap( blobHeap.begin(), blobHeap.end(), BlobValueGreaterCompare );

          localMinimalBestBlobValue = blobHeap.front().m_Value;
          }

        }

      ++n0, ++n1, ++n2;

      //progress.CompletedPixel();
      }
    } // end face list

  //  sort the heap, the first element will now be maximal
  std::sort_heap( blobHeap.begin(), blobHeap.end(), BlobValueGreaterCompare );

}

}

#endif // itkMultiScaleLaplacianBlobDetectorImageFilter2_hxx
