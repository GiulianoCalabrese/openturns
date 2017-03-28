//                                               -*- C++ -*-
/**
 *  @brief Design matrix cached evaluation
 *
 *  Copyright 2005-2017 Airbus-EDF-IMACS-Phimeca
 *
 *  This library is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  along with this library.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "openturns/Exception.hxx"
#include "openturns/PersistentObjectFactory.hxx"
#include "openturns/ResourceMap.hxx"
#include "openturns/DesignProxy.hxx"

BEGIN_NAMESPACE_OPENTURNS

CLASSNAMEINIT(DesignProxy);

/* Default constructor */
DesignProxy::DesignProxy ()
  : Object()
  , x_()
  , basis_()
  , designCache_(0, 0)
  , alreadyComputed_(0)
  , rowFilter_(0)
  , weight_(0)
{
  // Nothing to do
}

/* Parameters constructor */
DesignProxy::DesignProxy(const NumericalSample & x,
                         const Basis & basis)
  : Object()
  , x_(x)
  , basis_(basis)
  , designCache_(0, 0)
  , alreadyComputed_(0)
  , rowFilter_(0)
  , weight_(0)
{
  // keep initialization in the ctor for designCache_ to be shared among DesignProxy copies
  initialize();
}


void DesignProxy::initialize() const
{
  // allocate cache
  UnsignedInteger cacheSize = ResourceMap::GetAsUnsignedInteger("DesignProxy-DefaultCacheSize");
  UnsignedInteger nbRows = x_.getSize();
  if (nbRows == 0) throw InvalidArgumentException(HERE) << "Cannot initialize a DesignProxy with an empty sample";

  UnsignedInteger nbCols = cacheSize / nbRows;
  // The cache stores at least the first function values
  if (nbCols <= 0) nbCols = 1;
  designCache_ = Matrix(nbRows, nbCols);
  alreadyComputed_ = Indices(nbCols, 0);
}


/* Virtual constructor */
DesignProxy * DesignProxy::clone() const
{
  return new DesignProxy( *this );
}


/* String converter */
String DesignProxy::__repr__() const
{
  return OSS() << "class=" << GetClassName();
}


/* Compute the design matrix with the provided basis terms indices */
MatrixImplementation DesignProxy::computeDesign(const Indices & indices) const
{
  const UnsignedInteger cacheSize = designCache_.getNbColumns();
  const UnsignedInteger indicesSize = indices.getSize();
  const UnsignedInteger xSize = x_.getSize();
  MatrixImplementation design(xSize, indicesSize);
  MatrixImplementation::iterator startDesign(design.begin());
  for (UnsignedInteger j = 0; j < indicesSize; ++j)
  {
    const UnsignedInteger phiIndex = indices[j];
    // If the index is too large for the cache, compute the column and copy it directly in the design matrix
    if (phiIndex >= cacheSize)
    {
      const NumericalPoint column(basis_[phiIndex](x_).getImplementation()->getData());
      std::copy(column.begin(), column.end(), startDesign);
    } // Exceeds cache capacity
    else
    {
      MatrixImplementation::iterator startCache(designCache_.getImplementation()->begin() + phiIndex * xSize);
      // If the current index is already in the cache
      if (alreadyComputed_[phiIndex])
      {
        // Simply copy the cache content in the design matrix
        std::copy(startCache, startCache + xSize, startDesign);
      } // Values in the cache
      else
        // The value is not in the cache
      {
        // Compute the values
        NumericalPoint column(basis_[phiIndex](x_).getImplementation()->getData());
        // Copy the values in the cache
        alreadyComputed_[phiIndex] = 1;
        std::copy(column.begin(), column.end(), startCache);// copyOnWrite not called
        // And copy the value in the design matrix
        std::copy(column.begin(), column.end(), startDesign);
      } // values stored in the cache
    } // Index in cache capacity
    startDesign += xSize;
  } // j
  // Apply row filter if needed
  if (hasRowFilter())
  {
    const UnsignedInteger newRowDim = rowFilter_.getSize();
    MatrixImplementation filteredDesign(newRowDim, indicesSize);
    UnsignedInteger linearIndex = 0;
    UnsignedInteger shift = 0;
    for (UnsignedInteger j = 0; j < indicesSize; ++ j)
    {
      for (UnsignedInteger i = 0; i < newRowDim; ++ i)
      {
        filteredDesign[linearIndex] = design[shift + rowFilter_[i]];
        ++ linearIndex;
      }
      shift += xSize;
    }
    design = filteredDesign;
  } // hasRowFilter()
  if (hasWeight())
  {
    UnsignedInteger linearIndex = 0;
    for (UnsignedInteger j = 0; j < design.getNbColumns(); ++ j)
    {
      for (UnsignedInteger i = 0; i < design.getNbRows(); ++ i)
      {
        const UnsignedInteger newI = hasRowFilter() ? rowFilter_[i] : i;
        design[linearIndex] *= weight_[newI];
        ++ linearIndex;
      } // i
    } // j
  } // hasWeight()
  return design;
}

/* Input sample accessor */
NumericalSample DesignProxy::getInputSample() const
{
  return x_;
}

/* Basis accessor */
Basis DesignProxy::getBasis() const
{
  return basis_;
}

/* Row filter accessor */
void DesignProxy::setRowFilter(const Indices & rowFilter)
{
  rowFilter.check(x_.getSize());
  rowFilter_ = rowFilter;
}

Indices DesignProxy::getRowFilter() const
{
  return rowFilter_;
}

/* Row filter flag accessor */
Bool DesignProxy::hasRowFilter() const
{
  return rowFilter_.getSize() > 0;
}

/* Effective size of sample */
UnsignedInteger DesignProxy::getSampleSize() const
{
  return hasRowFilter() ? rowFilter_.getSize() : x_.getSize();
}


/* Weight accessor */
void DesignProxy::setWeight(const NumericalPoint & weight)
{
  if (weight.getDimension() > 0 && !(weight.getDimension() == x_.getSize())) throw InvalidArgumentException(HERE) << "Error: the weight dimension=" << weight.getDimension() << " is different from the sample size=" << x_.getSize();
  weight_ = weight;
}

NumericalPoint DesignProxy::getWeight() const
{
  return weight_;
}

/* Weight flag accessor */
Bool DesignProxy::hasWeight() const
{
  return weight_.getSize() > 0;
}

END_NAMESPACE_OPENTURNS
