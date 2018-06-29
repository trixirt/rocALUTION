#include "../../utils/def.hpp"
#include "hip_matrix_csr.hpp"
#include "hip_matrix_coo.hpp"
#include "hip_matrix_dia.hpp"
#include "hip_matrix_ell.hpp"
#include "hip_matrix_hyb.hpp"
#include "hip_matrix_mcsr.hpp"
#include "hip_matrix_bcsr.hpp"
#include "hip_matrix_dense.hpp"
#include "hip_vector.hpp"
#include "../host/host_matrix_dense.hpp"
#include "../base_matrix.hpp"
#include "../base_vector.hpp"
#include "../backend_manager.hpp"
#include "../../utils/log.hpp"
#include "hip_utils.hpp"
#include "hip_kernels_general.hpp"
#include "hip_kernels_dense.hpp"
#include "hip_allocate_free.hpp"
#include "hip_blas.hpp"
#include "../matrix_formats_ind.hpp"

#include <hip/hip_runtime.h>

namespace rocalution {

template <typename ValueType>
HIPAcceleratorMatrixDENSE<ValueType>::HIPAcceleratorMatrixDENSE() {

  // no default constructors
  LOG_INFO("no default constructor");
  FATAL_ERROR(__FILE__, __LINE__);

}

template <typename ValueType>
HIPAcceleratorMatrixDENSE<ValueType>::HIPAcceleratorMatrixDENSE(const Rocalution_Backend_Descriptor local_backend) {

  LOG_DEBUG(this, "HIPAcceleratorMatrixDENSE::HIPAcceleratorMatrixDENSE()",
            "constructor with local_backend");

  this->mat_.val = NULL;
  this->set_backend(local_backend); 

  CHECK_HIP_ERROR(__FILE__, __LINE__);

}


template <typename ValueType>
HIPAcceleratorMatrixDENSE<ValueType>::~HIPAcceleratorMatrixDENSE() {

  LOG_DEBUG(this, "HIPAcceleratorMatrixDENSE::~HIPAcceleratorMatrixDENSE()",
            "destructor");

  this->Clear();

}

template <typename ValueType>
void HIPAcceleratorMatrixDENSE<ValueType>::Info(void) const {

  LOG_INFO("HIPAcceleratorMatrixDENSE<ValueType>");

}

template <typename ValueType>
void HIPAcceleratorMatrixDENSE<ValueType>::AllocateDENSE(const int nrow, const int ncol) {

  assert( ncol  >= 0);
  assert( nrow  >= 0);

  if (this->GetNnz() > 0)
    this->Clear();

  if (nrow*ncol > 0) {

    allocate_hip(nrow*ncol, &this->mat_.val);
    set_to_zero_hip(this->local_backend_.HIP_block_size, 
                    nrow*ncol, mat_.val);   

    this->nrow_ = nrow;
    this->ncol_ = ncol;
    this->nnz_  = nrow*ncol;

  }


}

template <typename ValueType>
void HIPAcceleratorMatrixDENSE<ValueType>::Clear() {

  if (this->GetNnz() > 0) {

    free_hip(&this->mat_.val);

    this->nrow_ = 0;
    this->ncol_ = 0;
    this->nnz_  = 0;

  }

}

template <typename ValueType>
void HIPAcceleratorMatrixDENSE<ValueType>::SetDataPtrDENSE(ValueType **val, const int nrow, const int ncol) {

  assert(*val != NULL);
  assert(nrow > 0);
  assert(ncol > 0);

  this->Clear();

  hipDeviceSynchronize();

  this->nrow_ = nrow;
  this->ncol_ = ncol;
  this->nnz_  = nrow*ncol;

  this->mat_.val = *val;

}

template <typename ValueType>
void HIPAcceleratorMatrixDENSE<ValueType>::LeaveDataPtrDENSE(ValueType **val) {

  assert(this->nrow_ > 0);
  assert(this->ncol_ > 0);
  assert(this->nnz_ > 0);
  assert(this->nnz_  == this->nrow_*this->ncol_);

  hipDeviceSynchronize();

  *val = this->mat_.val;

  this->mat_.val = NULL;

  this->nrow_ = 0;
  this->ncol_ = 0;
  this->nnz_  = 0;

}

template <typename ValueType>
void HIPAcceleratorMatrixDENSE<ValueType>::CopyFromHost(const HostMatrix<ValueType> &src) {

  const HostMatrixDENSE<ValueType> *cast_mat;

  // copy only in the same format
  assert(this->GetMatFormat() == src.GetMatFormat());

  // CPU to HIP copy
  if ((cast_mat = dynamic_cast<const HostMatrixDENSE<ValueType>*> (&src)) != NULL) {
    
  if (this->GetNnz() == 0)
    this->AllocateDENSE(src.GetM(), src.GetN() );

    assert(this->GetNnz()  == src.GetNnz());
    assert(this->GetM() == src.GetM());
    assert(this->GetN() == src.GetN());

    if (this->GetNnz() > 0) {

      hipMemcpy(this->mat_.val,     // dst
                 cast_mat->mat_.val, // src
                 this->GetNnz()*sizeof(ValueType), // size
                 hipMemcpyHostToDevice);    
      CHECK_HIP_ERROR(__FILE__, __LINE__);     
    }
    
  } else {
    
    LOG_INFO("Error unsupported HIP matrix type");
    this->Info();
    src.Info();
    FATAL_ERROR(__FILE__, __LINE__);
    
  }

}

template <typename ValueType>
void HIPAcceleratorMatrixDENSE<ValueType>::CopyToHost(HostMatrix<ValueType> *dst) const {

  HostMatrixDENSE<ValueType> *cast_mat;

  // copy only in the same format
  assert(this->GetMatFormat() == dst->GetMatFormat());

  // HIP to CPU copy
  if ((cast_mat = dynamic_cast<HostMatrixDENSE<ValueType>*> (dst)) != NULL) {

    cast_mat->set_backend(this->local_backend_);   

  if (dst->GetNnz() == 0)
    cast_mat->AllocateDENSE(this->GetM(), this->GetN() );

    assert(this->GetNnz()  == dst->GetNnz());
    assert(this->GetM() == dst->GetM());
    assert(this->GetN() == dst->GetN());

    if (this->GetNnz() > 0) {
      
      hipMemcpy(cast_mat->mat_.val, // dst
                 this->mat_.val,     // src
                 this->GetNnz()*sizeof(ValueType), // size
                 hipMemcpyDeviceToHost);    
      CHECK_HIP_ERROR(__FILE__, __LINE__);     
    }
    
  } else {
    
    LOG_INFO("Error unsupported HIP matrix type");
    this->Info();
    dst->Info();
    FATAL_ERROR(__FILE__, __LINE__);
    
  }

}

template <typename ValueType>
void HIPAcceleratorMatrixDENSE<ValueType>::CopyFrom(const BaseMatrix<ValueType> &src) {

  const HIPAcceleratorMatrixDENSE<ValueType> *hip_cast_mat;
  const HostMatrix<ValueType> *host_cast_mat;

  // copy only in the same format
  assert(this->GetMatFormat() == src.GetMatFormat());

  // HIP to HIP copy
  if ((hip_cast_mat = dynamic_cast<const HIPAcceleratorMatrixDENSE<ValueType>*> (&src)) != NULL) {
    
  if (this->GetNnz() == 0)
    this->AllocateDENSE(src.GetM(), src.GetN() );

    assert(this->GetNnz()  == src.GetNnz());
    assert(this->GetM() == src.GetM());
    assert(this->GetN() == src.GetN());

    if (this->GetNnz() > 0) { 

        hipMemcpy(this->mat_.val,         // dst
                   hip_cast_mat->mat_.val, // src
                   this->GetNnz()*sizeof(ValueType), // size
                   hipMemcpyDeviceToDevice);    
        CHECK_HIP_ERROR(__FILE__, __LINE__);     
      }

  } else {

    //CPU to HIP
    if ((host_cast_mat = dynamic_cast<const HostMatrix<ValueType>*> (&src)) != NULL) {
      
      this->CopyFromHost(*host_cast_mat);
      
    } else {
      
      LOG_INFO("Error unsupported HIP matrix type");
      this->Info();
      src.Info();
      FATAL_ERROR(__FILE__, __LINE__);
      
    }
    
  }

}

template <typename ValueType>
void HIPAcceleratorMatrixDENSE<ValueType>::CopyTo(BaseMatrix<ValueType> *dst) const {

  HIPAcceleratorMatrixDENSE<ValueType> *hip_cast_mat;
  HostMatrix<ValueType> *host_cast_mat;

  // copy only in the same format
  assert(this->GetMatFormat() == dst->GetMatFormat());

  // HIP to HIP copy
  if ((hip_cast_mat = dynamic_cast<HIPAcceleratorMatrixDENSE<ValueType>*> (dst)) != NULL) {

    hip_cast_mat->set_backend(this->local_backend_);       

  if (this->GetNnz() == 0)
    hip_cast_mat->AllocateDENSE(dst->GetM(), dst->GetN() );

    assert(this->GetNnz()  == dst->GetNnz());
    assert(this->GetM() == dst->GetM());
    assert(this->GetN() == dst->GetN());

    if (this->GetNnz() > 0) {

        hipMemcpy(hip_cast_mat->mat_.val, // dst
                   this->mat_.val,         // src
                   this->GetNnz()*sizeof(ValueType), // size
                   hipMemcpyDeviceToHost);    
        CHECK_HIP_ERROR(__FILE__, __LINE__);     
      }
    
  } else {

    //HIP to CPU
    if ((host_cast_mat = dynamic_cast<HostMatrix<ValueType>*> (dst)) != NULL) {
      
      this->CopyToHost(host_cast_mat);

    } else {
      
      LOG_INFO("Error unsupported HIP matrix type");
      this->Info();
      dst->Info();
      FATAL_ERROR(__FILE__, __LINE__);
      
    }

  }


}


template <typename ValueType>
void HIPAcceleratorMatrixDENSE<ValueType>::CopyFromHostAsync(const HostMatrix<ValueType> &src) {

  const HostMatrixDENSE<ValueType> *cast_mat;

  // copy only in the same format
  assert(this->GetMatFormat() == src.GetMatFormat());

  // CPU to HIP copy
  if ((cast_mat = dynamic_cast<const HostMatrixDENSE<ValueType>*> (&src)) != NULL) {
    
  if (this->GetNnz() == 0)
    this->AllocateDENSE(src.GetM(), src.GetN() );

    assert(this->GetNnz()  == src.GetNnz());
    assert(this->GetM() == src.GetM());
    assert(this->GetN() == src.GetN());

    if (this->GetNnz() > 0) {

      hipMemcpyAsync(this->mat_.val,     // dst
                      cast_mat->mat_.val, // src
                      this->GetNnz()*sizeof(ValueType), // size
                      hipMemcpyHostToDevice);    
      CHECK_HIP_ERROR(__FILE__, __LINE__);     
    }
    
  } else {
    
    LOG_INFO("Error unsupported HIP matrix type");
    this->Info();
    src.Info();
    FATAL_ERROR(__FILE__, __LINE__);
    
  }

}

template <typename ValueType>
void HIPAcceleratorMatrixDENSE<ValueType>::CopyToHostAsync(HostMatrix<ValueType> *dst) const {

  HostMatrixDENSE<ValueType> *cast_mat;

  // copy only in the same format
  assert(this->GetMatFormat() == dst->GetMatFormat());

  // HIP to CPU copy
  if ((cast_mat = dynamic_cast<HostMatrixDENSE<ValueType>*> (dst)) != NULL) {

    cast_mat->set_backend(this->local_backend_);   

  if (dst->GetNnz() == 0)
    cast_mat->AllocateDENSE(this->GetM(), this->GetN() );

    assert(this->GetNnz()  == dst->GetNnz());
    assert(this->GetM() == dst->GetM());
    assert(this->GetN() == dst->GetN());

    if (this->GetNnz() > 0) {
      
      hipMemcpyAsync(cast_mat->mat_.val, // dst
                      this->mat_.val,     // src
                      this->GetNnz()*sizeof(ValueType), // size
                      hipMemcpyDeviceToHost);    
      CHECK_HIP_ERROR(__FILE__, __LINE__);     
    }
    
  } else {
    
    LOG_INFO("Error unsupported HIP matrix type");
    this->Info();
    dst->Info();
    FATAL_ERROR(__FILE__, __LINE__);
    
  }

}

template <typename ValueType>
void HIPAcceleratorMatrixDENSE<ValueType>::CopyFromAsync(const BaseMatrix<ValueType> &src) {

  const HIPAcceleratorMatrixDENSE<ValueType> *hip_cast_mat;
  const HostMatrix<ValueType> *host_cast_mat;

  // copy only in the same format
  assert(this->GetMatFormat() == src.GetMatFormat());

  // HIP to HIP copy
  if ((hip_cast_mat = dynamic_cast<const HIPAcceleratorMatrixDENSE<ValueType>*> (&src)) != NULL) {
    
  if (this->GetNnz() == 0)
    this->AllocateDENSE(src.GetM(), src.GetN() );

    assert(this->GetNnz()  == src.GetNnz());
    assert(this->GetM() == src.GetM());
    assert(this->GetN() == src.GetN());

    if (this->GetNnz() > 0) { 

      hipMemcpy(this->mat_.val,         // dst
                 hip_cast_mat->mat_.val, // src
                 this->GetNnz()*sizeof(ValueType), // size
                 hipMemcpyDeviceToDevice);    
        CHECK_HIP_ERROR(__FILE__, __LINE__);     
      }

  } else {

    //CPU to HIP
    if ((host_cast_mat = dynamic_cast<const HostMatrix<ValueType>*> (&src)) != NULL) {
      
      this->CopyFromHostAsync(*host_cast_mat);
      
    } else {
      
      LOG_INFO("Error unsupported HIP matrix type");
      this->Info();
      src.Info();
      FATAL_ERROR(__FILE__, __LINE__);
      
    }
    
  }

}

template <typename ValueType>
void HIPAcceleratorMatrixDENSE<ValueType>::CopyToAsync(BaseMatrix<ValueType> *dst) const {

  HIPAcceleratorMatrixDENSE<ValueType> *hip_cast_mat;
  HostMatrix<ValueType> *host_cast_mat;

  // copy only in the same format
  assert(this->GetMatFormat() == dst->GetMatFormat());

  // HIP to HIP copy
  if ((hip_cast_mat = dynamic_cast<HIPAcceleratorMatrixDENSE<ValueType>*> (dst)) != NULL) {

    hip_cast_mat->set_backend(this->local_backend_);       

  if (this->GetNnz() == 0)
    hip_cast_mat->AllocateDENSE(dst->GetM(), dst->GetN() );

    assert(this->GetNnz()  == dst->GetNnz());
    assert(this->GetM() == dst->GetM());
    assert(this->GetN() == dst->GetN());

    if (this->GetNnz() > 0) {

        hipMemcpy(hip_cast_mat->mat_.val, // dst
                   this->mat_.val,         // src
                   this->GetNnz()*sizeof(ValueType), // size
                   hipMemcpyDeviceToHost);    
        CHECK_HIP_ERROR(__FILE__, __LINE__);     
      }
    
  } else {

    //HIP to CPU
    if ((host_cast_mat = dynamic_cast<HostMatrix<ValueType>*> (dst)) != NULL) {
      
      this->CopyToHostAsync(host_cast_mat);

    } else {
      
      LOG_INFO("Error unsupported HIP matrix type");
      this->Info();
      dst->Info();
      FATAL_ERROR(__FILE__, __LINE__);
      
    }

  }


}


template <typename ValueType>
bool HIPAcceleratorMatrixDENSE<ValueType>::ConvertFrom(const BaseMatrix<ValueType> &mat) {

  this->Clear();

  // empty matrix is empty matrix
  if (mat.GetNnz() == 0)
    return true;

  const HIPAcceleratorMatrixDENSE<ValueType>   *cast_mat_dense;
  
  if ((cast_mat_dense = dynamic_cast<const HIPAcceleratorMatrixDENSE<ValueType>*> (&mat)) != NULL) {

      this->CopyFrom(*cast_mat_dense);
      return true;

  }

  /*
  const HIPAcceleratorMatrixCSR<ValueType>   *cast_mat_csr;
  if ((cast_mat_csr = dynamic_cast<const HIPAcceleratorMatrixCSR<ValueType>*> (&mat)) != NULL) {
    
    this->Clear();
    
    FATAL_ERROR(__FILE__, __LINE__);
    
    this->nrow_ = cast_mat_csr->GetM();
    this->ncol_ = cast_mat_csr->GetN();
    this->nnz_  = cast_mat_csr->GetNnz();
    
    return true;
    
  }
  */

  return false;

}

template <typename ValueType>
void HIPAcceleratorMatrixDENSE<ValueType>::Apply(const BaseVector<ValueType> &in, BaseVector<ValueType> *out) const {

  if (this->GetNnz() > 0) {

    assert(in.  GetSize() >= 0);
    assert(out->GetSize() >= 0);
    assert(in.  GetSize() == this->GetN());
    assert(out->GetSize() == this->GetM());

    const HIPAcceleratorVector<ValueType> *cast_in = dynamic_cast<const HIPAcceleratorVector<ValueType>*> (&in);
    HIPAcceleratorVector<ValueType> *cast_out      = dynamic_cast<      HIPAcceleratorVector<ValueType>*> (out);

    assert(cast_in != NULL);
    assert(cast_out!= NULL);

    hipblasStatus_t status;

    const ValueType alpha = static_cast<ValueType>(1);
    const ValueType beta  = static_cast<ValueType>(0);

    if (DENSE_IND_BASE == 0) {

      status = hipblasTgemv(HIPBLAS_HANDLE(this->local_backend_.HIP_blas_handle),
                            HIPBLAS_OP_N,
                            this->GetM(), this->GetN(),
                            &alpha,
                            this->mat_.val, this->GetM(),
                            cast_in->vec_, 1,
                            &beta, cast_out->vec_, 1);
      CHECK_HIPBLAS_ERROR(status, __FILE__, __LINE__);

    } else {

      status = hipblasTgemv(HIPBLAS_HANDLE(this->local_backend_.HIP_blas_handle),
                            HIPBLAS_OP_T,
                            this->GetN(), this->GetM(),
                            &alpha,
                            this->mat_.val, this->GetN(),
                            cast_in->vec_, 1,
                            &beta, cast_out->vec_, 1);
      CHECK_HIPBLAS_ERROR(status, __FILE__, __LINE__);

    }

  }

}

template <typename ValueType>
void HIPAcceleratorMatrixDENSE<ValueType>::ApplyAdd(const BaseVector<ValueType> &in, const ValueType scalar,
                                                    BaseVector<ValueType> *out) const {

  if (this->GetNnz() > 0) {

    assert(in.  GetSize() >= 0);
    assert(out->GetSize() >= 0);
    assert(in.  GetSize() == this->GetN());
    assert(out->GetSize() == this->GetM());

    const HIPAcceleratorVector<ValueType> *cast_in = dynamic_cast<const HIPAcceleratorVector<ValueType>*> (&in);
    HIPAcceleratorVector<ValueType> *cast_out      = dynamic_cast<      HIPAcceleratorVector<ValueType>*> (out);

    assert(cast_in != NULL);
    assert(cast_out!= NULL);

    hipblasStatus_t status;

    const ValueType beta  = static_cast<ValueType>(0);

    if (DENSE_IND_BASE == 0) {

      status = hipblasTgemv(HIPBLAS_HANDLE(this->local_backend_.HIP_blas_handle),
                            HIPBLAS_OP_N,
                            this->GetM(), this->GetN(),
                            &scalar,
                            this->mat_.val, this->GetM(),
                            cast_in->vec_, 1,
                            &beta, cast_out->vec_, 1);
      CHECK_HIPBLAS_ERROR(status, __FILE__, __LINE__);

    } else {

      status = hipblasTgemv(HIPBLAS_HANDLE(this->local_backend_.HIP_blas_handle),
                            HIPBLAS_OP_T,
                            this->GetN(), this->GetM(),
                            &scalar,
                            this->mat_.val, this->GetN(),
                            cast_in->vec_, 1,
                            &beta, cast_out->vec_, 1);
      CHECK_HIPBLAS_ERROR(status, __FILE__, __LINE__);

    }

  }

}

template <typename ValueType>
bool HIPAcceleratorMatrixDENSE<ValueType>::MatMatMult(const BaseMatrix<ValueType> &A, const BaseMatrix<ValueType> &B) {

  assert((this != &A) && (this != &B));

  const HIPAcceleratorMatrixDENSE<ValueType> *cast_mat_A = dynamic_cast<const HIPAcceleratorMatrixDENSE<ValueType>*> (&A);
  const HIPAcceleratorMatrixDENSE<ValueType> *cast_mat_B = dynamic_cast<const HIPAcceleratorMatrixDENSE<ValueType>*> (&B);

  assert(cast_mat_A != NULL);
  assert(cast_mat_B != NULL);
  assert(cast_mat_A->ncol_ == cast_mat_B->nrow_);

  hipblasStatus_t status;

  const ValueType alpha = static_cast<ValueType>(1);
  const ValueType beta  = static_cast<ValueType>(0);

  if (DENSE_IND_BASE == 0) {

    status = hipblasTgemm(HIPBLAS_HANDLE(this->local_backend_.HIP_blas_handle),
                          HIPBLAS_OP_N, HIPBLAS_OP_N,
                          cast_mat_A->nrow_, cast_mat_B->ncol_, cast_mat_A->ncol_,
                          &alpha, cast_mat_A->mat_.val, cast_mat_A->nrow_,
                          cast_mat_B->mat_.val, cast_mat_A->ncol_, &beta,
                          this->mat_.val, cast_mat_A->nrow_);
    CHECK_HIPBLAS_ERROR(status, __FILE__, __LINE__);

  } else {

    status = hipblasTgemm(HIPBLAS_HANDLE(this->local_backend_.HIP_blas_handle),
                          HIPBLAS_OP_T, HIPBLAS_OP_T,
                          cast_mat_A->nrow_, cast_mat_B->ncol_, cast_mat_A->ncol_,
                          &alpha, cast_mat_A->mat_.val, cast_mat_A->ncol_,
                          cast_mat_B->mat_.val, cast_mat_B->ncol_, &beta,
                          this->mat_.val, cast_mat_A->nrow_);
    CHECK_HIPBLAS_ERROR(status, __FILE__, __LINE__);

  }

  return true;

}

template <typename ValueType>
bool HIPAcceleratorMatrixDENSE<ValueType>::ReplaceColumnVector(const int idx, const BaseVector<ValueType> &vec) {

  assert(vec.GetSize() == this->GetM());

  if (this->GetNnz() > 0) {

    const HIPAcceleratorVector<ValueType> *cast_vec = dynamic_cast<const HIPAcceleratorVector<ValueType>*> (&vec);
    assert(cast_vec != NULL);

    const int nrow = this->GetM();
    const int ncol = this->GetN();

    dim3 BlockSize(this->local_backend_.HIP_block_size);
    dim3 GridSize(nrow / this->local_backend_.HIP_block_size + 1);

    hipLaunchKernelGGL((kernel_dense_replace_column_vector<ValueType, int>),
                       GridSize, BlockSize, 0, 0,
                       cast_vec->vec_, idx, nrow, ncol, this->mat_.val);
    CHECK_HIP_ERROR(__FILE__,__LINE__);

  }

  return true;

}

template <typename ValueType>
bool HIPAcceleratorMatrixDENSE<ValueType>::ReplaceRowVector(const int idx, const BaseVector<ValueType> &vec) {

  assert(vec.GetSize() == this->GetN());

  if (this->GetNnz() > 0) {

    const HIPAcceleratorVector<ValueType> *cast_vec = dynamic_cast<const HIPAcceleratorVector<ValueType>*> (&vec);
    assert(cast_vec != NULL);

    const int nrow = this->GetM();
    const int ncol = this->GetN();

    dim3 BlockSize(this->local_backend_.HIP_block_size);
    dim3 GridSize(ncol / this->local_backend_.HIP_block_size + 1);

    hipLaunchKernelGGL((kernel_dense_replace_row_vector<ValueType, int>),
                       GridSize, BlockSize, 0, 0,
                       cast_vec->vec_, idx, nrow, ncol, this->mat_.val);
    CHECK_HIP_ERROR(__FILE__,__LINE__);

  }

  return true;

}

template <typename ValueType>
bool HIPAcceleratorMatrixDENSE<ValueType>::ExtractColumnVector(const int idx, BaseVector<ValueType> *vec) const {

    assert(vec != NULL);
    assert(vec->GetSize() == this->GetM());

  if (this->GetNnz() > 0) {

    HIPAcceleratorVector<ValueType> *cast_vec = dynamic_cast<HIPAcceleratorVector<ValueType>*> (vec);
    assert(cast_vec != NULL);

    const int nrow = this->GetM();
    const int ncol = this->GetN();

    dim3 BlockSize(this->local_backend_.HIP_block_size);
    dim3 GridSize(nrow / this->local_backend_.HIP_block_size + 1);

    hipLaunchKernelGGL((kernel_dense_extract_column_vector<ValueType, int>),
                       GridSize, BlockSize, 0, 0,
                       cast_vec->vec_, idx, nrow, ncol, this->mat_.val);
    CHECK_HIP_ERROR(__FILE__,__LINE__);

  }

  return true;

}

template <typename ValueType>
bool HIPAcceleratorMatrixDENSE<ValueType>::ExtractRowVector(const int idx, BaseVector<ValueType> *vec) const {

    assert(vec != NULL);
    assert(vec->GetSize() == this->GetN());

  if (this->GetNnz() > 0) {

    HIPAcceleratorVector<ValueType> *cast_vec = dynamic_cast<HIPAcceleratorVector<ValueType>*> (vec);
    assert(cast_vec != NULL);

    const int nrow = this->GetM();
    const int ncol = this->GetN();

    dim3 BlockSize(this->local_backend_.HIP_block_size);
    dim3 GridSize(ncol / this->local_backend_.HIP_block_size + 1);

    hipLaunchKernelGGL((kernel_dense_extract_row_vector<ValueType, int>),
                       GridSize, BlockSize, 0, 0,
                       cast_vec->vec_, idx, nrow, ncol, this->mat_.val);
    CHECK_HIP_ERROR(__FILE__,__LINE__);

  }

  return true;

}


template class HIPAcceleratorMatrixDENSE<double>;
template class HIPAcceleratorMatrixDENSE<float>;
#ifdef SUPPORT_COMPLEX
template class HIPAcceleratorMatrixDENSE<std::complex<double> >;
template class HIPAcceleratorMatrixDENSE<std::complex<float> >;
#endif

}