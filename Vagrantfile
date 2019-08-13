# -*- mode: ruby -*-
# vi: set ft=ruby :

Vagrant.configure("2") do |config|
  config.vm.provider :virtualbox do |vb|
    vb.memory = 2048
  end
  config.vm.provision "shell", path: "setup/provision.sh"
  config.ssh.forward_agent = true
  config.ssh.forward_x11 = true

  config.vm.define "icc",  autostart: false do |icc|
    icc.vm.box = "centos/7"
    icc.vm.synced_folder ".", "/vagrant", type: "nfs"
    icc.vm.network "private_network", ip: "172.16.200.2"  # needed for NFS
    icc.vm.network "forwarded_port", guest: 7624, host: 7625
  end

  config.vm.define "aoc", primary: true do |aoc|
    aoc.vm.box = "generic/ubuntu1804"
    aoc.vm.synced_folder ".", "/vagrant"
    aoc.vm.network "private_network", ip: "172.16.200.3"  # needed for NFS
    aoc.vm.network "forwarded_port", guest: 7624, host: 7625
  end
end
