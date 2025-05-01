## Example of a manual MagAOX installation on an existing system:

### Role and sources

* MagAOX role:
  * **magaox01**
  * Initial role for provisioning:   **workstation**
* MagAOX source:
  * Repository => **https://github.com/drbitboy/MagAOX.git**
  * Branch => **resurrector-20250428-resuctrl-xsup-permissions**
* MagAOX configuration source:
  * Repository => **https://github.com/drbitboy/magao-x-config.git**
  * Branch => **resurrector-indi-compression**

### Initial setup

* As sudo-capable user:
```
sudo apt update
sudo apt upgrade -y
sudo reboot
```

### Get sources
* As sudo-capable user:
```
cd
mkdir githubalt

git clone \
  --depth=1 \
  -b resurrector-20250428-resuctrl-xsup-pemissions \
  https://github.com/drbitboy/MagAOX.git \
  githubalt/MagAOX

git clone \
  --depth=1 \
  -b resurrector-indi-compression \
  https://github.com/drbitboy/magao-x-config.git \
  githubalt/config
```

### Provision MagAOX system
* As sudo-capable user:
```
cd
cd githubalt/MagAOX/setup
MAGAOX_ROLE=workstation ./pre_provision.sh
source /etc/profile.d/magaox_role.sh
bash ./provision.sh"
```

### Change system role to **magaox01**
* As sudo-capable user:
```
echo MAGAOX_ROLE=magaox01 | sudo tee /etc/profile.d/magaox_role.sh
source /etc/profile.d/magaox_role.sh
```
## End of manual installation
___
___
## Single-command example of automated creation new Multipass VMs, plus installation and provisioning of MagAOX on same:

### Create and provision multipass VM named magaox01, plus second cloned VM named magaox02

* N.B. Roles will change to magaox01 and magaox02 after provisioning
```
./magaox_multipass_setup.sh \
  -v=magaox01 -2=magaox02 \
  -M=drbitboy/MagAOX,MagAOX,resurrector-20250428-resuctrl-xsup-permissions \
  -r=drbitboy/magao-x-config,config,resurrector-indi-compression
```
## End of automated installation
___
___
## Running MagAOX system, assuming either installation procedure above is complete

### Cache process list file for use by non-privileged user **xsup**; create Hexbeater FIFOs and INDI driver/server system directories (/opt/MagAOX/sys/.../); su to non-privileged user **xsup** to run MagAOX system

* As sudo-capable user on both/either magaox01 and magaox02:
```
resuctrl reset   ### Creation, provisioning are complete after this step
sudo su - xsup
```

### Start INDI server and all INDI drivers under resurrector control
### Update maths_1 and maths_2 INDI drivers, view results

* As user xsup:
```
resuctrl startup
resuctrl status --all

setINDI maths_1.val.value=1.1   ### on magaox01
setINDI maths_2.val.value=1.2   ### on magaox02

getINDI -t 1
```

### Stop an INDI driver

```
resuctrl stop maths_1           ### on magaox01
resuctrl status                 ### --all is implied
```

### Start an INDI driver

```
resuctrl start maths_1          ### on magaox01
resuctrl status                 ### --all is implied
```

### Stop everything

```
resuctrl stop --all
```
