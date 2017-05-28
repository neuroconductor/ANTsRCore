% Generated by roxygen2: do not edit by hand
% Please edit documentation in R/coerce_mask.R
\name{coerce_mask}
\alias{coerce_mask}
\title{Simple Mask Maker}
\usage{
coerce_mask(x, error = TRUE)
}
\arguments{
\item{x}{antsImage object}

\item{error}{should the function error if the mask as not binary?}
}
\value{
Array, logical if all values within 0, 1, \code{NA}, and \code{NaN}
}
\description{
Creates a logical array from an antsImage object
}