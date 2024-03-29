% Generated by roxygen2: do not edit by hand
% Please edit documentation in R/n4BiasFieldCorrection.R
\name{n4BiasFieldCorrection}
\alias{n4BiasFieldCorrection}
\title{N4 bias field correction}
\usage{
n4BiasFieldCorrection(
  img,
  mask,
  shrinkFactor = 4,
  convergence = list(iters = c(50, 50, 50, 50), tol = 1e-07),
  splineParam = 200,
  returnBiasField = FALSE,
  verbose = FALSE,
  weight_mask = NULL
)
}
\arguments{
\item{img}{input antsImage}

\item{mask}{input mask, if one is not passed one will be made}

\item{shrinkFactor}{Shrink factor for multi-resolution correction,
typically integer less than 4}

\item{convergence}{List of:  \code{iters}, vector of maximum number of
iterations for each shrinkage factor, and \code{tol}, the convergence tolerance.}

\item{splineParam}{Parameter controlling number of control points in spline.
Either single value, indicating how many control points, or vector
with one entry per dimension of image, indicating the spacing in each direction.}

\item{returnBiasField}{bool, return the field instead of the corrected image.}

\item{verbose}{enables verbose output.}

\item{weight_mask}{antsImage of weight mask}
}
\value{
bias corrected image or bias field
}
\description{
Perform N4 bias field correction on the given image
}
\examples{
 dims = c(50, 50)
 img<-makeImage(imagesize = dims, rnorm(prod(dims)) )
 n4img<-n4BiasFieldCorrection(img)
 n4img<-n4BiasFieldCorrection(img, mask = img > 0)
 testthat::expect_error(n4BiasFieldCorrection(img, weight_mask = "somepath"))
 testthat::expect_error(n4BiasFieldCorrection(img, splineParam = rep(200, 3)))
 n4img<-n4BiasFieldCorrection(img, splineParam = c(200, 20))

 rm(img); gc()
 rm(n4img); gc()
fname = getANTsRData("r16")
in_img = antsImageRead(fname)
n4 = n4BiasFieldCorrection(in_img)
rm(n4); gc()
mask = in_img > 0
mask2 = antsImageClone(mask, out_pixeltype = "float")
# fails
mask
sum(mask)
\dontrun{
n4 = n4BiasFieldCorrection(in_img, mask = mask, verbose = TRUE)
# fails
n4 = n4BiasFieldCorrection(in_img, mask = mask2)
}
}
\author{
BB Avants
}
