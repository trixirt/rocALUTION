%global upstreamname rocALUTION
%global rocm_release 5.6
%global rocm_patch 0
%global rocm_version %{rocm_release}.%{rocm_patch}

# Compiler is hipcc, which is clang based:
%global toolchain clang
# hipcc does not support some clang flags
%global build_cxxflags %(echo %{optflags} | sed -e 's/-fstack-protector-strong/-Xarch_host -fstack-protector-strong/' -e 's/-fcf-protection/-Xarch_host -fcf-protection/')

Name:           rocblas
Version:        %{rocm_version}
Release:        1%{?dist}
Summary:        BLAS implementation for ROCm
Url:            https://github.com/ROCmSoftwarePlatform
License:        MIT

Source0:        %{url}/%{upstreamname}/archive/refs/tags/rocm-%{version}.tar.gz#/%{upstreamname}-%{version}.tar.gz

BuildRequires:  cmake
BuildRequires:  clang-devel
BuildRequires:  compiler-rt
BuildRequires:  lld
BuildRequires:  llvm-devel
BuildRequires:  rocblas-devel
BuildRequires:  rocm-cmake
BuildRequires:  rocm-comgr-devel
BuildRequires:  rocm-hip-devel
BuildRequires:  rocm-runtime-devel
BuildRequires:  rocrand-devel
BuildRequires:  rocsparse-devel

%description
TBD

%package devel
Summary:        TBD

%description devel
%{summary}

%prep
%autosetup -p1 -n %{upstreamname}-rocm-%{version}

%build
%cmake \
       -DCMAKE_C_COMPILER=hipcc \
       -DCMAKE_CXX_COMPILER=hipcc

      
%cmake_build

%install
%cmake_install

%files devel


%changelog
* Wed Aug 16 2023 Tom Rix <trix@redhat.com>
- Stub something together
