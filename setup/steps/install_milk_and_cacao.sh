#!/bin/bash
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source $DIR/../_common.sh
set -eo pipefail

COMMIT_ISH=dev
orgname=milk-org
reponame=milk
parentdir=/opt/MagAOX/source
clone_or_update_and_cd $orgname $reponame $parentdir
git checkout $COMMIT_ISH

bash -x ./fetch_cacao_dev.sh

sudo rm -rf _build
mkdir -p _build
cd _build

pythonExe=/opt/conda/bin/python
$pythonExe -m pip install pybind11

milkCmakeArgs="-Dbuild_python_module=ON -DPYTHON_EXECUTABLE=${pythonExe}"

if [[ $MAGAOX_ROLE == TIC || $MAGAOX_ROLE == ICC || $MAGAOX_ROLE == RTC || $MAGAOX_ROLE == AOC ]]; then
    milkCmakeArgs="-DUSE_CUDA=ON ${milkCmakeArgs}"
fi

cmake .. $milkCmakeArgs
make
sudo make install

sudo $pythonExe -m pip install -e ../src/ImageStreamIO/
$pythonExe -c 'import ImageStreamIOWrap' || exit 1

milkSuffix=bin/milk
milkBinary=$(grep -e "${milkSuffix}$" ./install_manifest.txt)
milkPath=${milkBinary/${milkSuffix}/}

if command -v milk; then
    log_warn "Found existing milk binary at $(command -v milk)"
fi
sudo ln -sf $milkPath /usr/local/milk
echo "/usr/local/milk/lib" | sudo tee /etc/ld.so.conf.d/milk.conf
sudo ldconfig
echo "export PATH=\"\$PATH:/usr/local/milk/bin\"" | sudo tee /etc/profile.d/milk.sh
echo "export PKG_CONFIG_PATH=\$PKG_CONFIG_PATH:/usr/local/milk/lib/pkgconfig" | sudo tee -a /etc/profile.d/milk.sh
echo "export MILK_SHM_DIR=/milk/shm" | sudo tee -a /etc/profile.d/milk.sh

if [[ $MAGAOX_ROLE != ci ]]; then
  if ! grep -q "/milk/shm" /etc/fstab; then
    echo "tmpfs /milk/shm tmpfs rw,nosuid,nodev" | sudo tee -a /etc/fstab
    sudo mkdir -p /milk/shm
    log_success "Created /milk/shm tmpfs mountpoint"
    sudo mount /milk/shm
    log_success "Mounted /milk/shm"
  else
    log_info "Skipping /milk/shm mount setup"
  fi
fi
if [[ $MAGAOX_ROLE == ICC || $MAGAOX_ROLE == RTC ]]; then
  clone_or_update_and_cd magao-x "cacao-${MAGAOX_ROLE,,}" /opt/MagAOX
  sudo ln -s "/opt/MagAOX/cacao-${MAGAOX_ROLE,,}" /opt/MagAOX/cacao
  sudo install $DIR/../systemd_units/cacao_startup.service /etc/systemd/system/
  sudo systemctl daemon-reload || true
  sudo systemctl enable cacao_startup.service || true
fi