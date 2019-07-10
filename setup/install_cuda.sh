#!/bin/bash
set -exuo pipefail
CUDA_RPM_DIR=$DEPSROOT/cuda
mkdir -p $CUDA_RPM_DIR
cd $CUDA_RPM_DIR
CUDA_RPM_FILE="cuda-repo-rhel7-10-1-local-10.1.168-418.67-1.0-1.x86_64.rpm"
# Version with dots changed to dashes for package names
CUDA_VERSION_SPEC="10-1"
if [[ ! -e $CUDA_RPM_FILE ]]; then
    curl -OL "https://developer.nvidia.com/compute/cuda/10.1/Prod/local_installers/$CUDA_RPM_FILE"
fi
rpm -i $CUDA_RPM_FILE || true
yum clean all
yum install -y cuda-toolkit-$CUDA_VERSION_SPEC \
    cuda-tools-$CUDA_VERSION_SPEC \
    cuda-runtime-$CUDA_VERSION_SPEC \
    cuda-compiler-$CUDA_VERSION_SPEC \
    cuda-libraries-$CUDA_VERSION_SPEC \
    cuda-libraries-dev-$CUDA_VERSION_SPEC \
    cuda-drivers
echo "export CUDADIR=/usr/local/cuda" > /etc/profile.d/cuda.sh
echo "export PATH=\"\$PATH:/usr/local/cuda/bin\"" >> /etc/profile.d/cuda.sh
