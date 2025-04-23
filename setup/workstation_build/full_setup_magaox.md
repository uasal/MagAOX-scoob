## As some user on multipass host OS:

### Create, provision multipass VM named magaox01, plus second cloned VM named magaox02

* N.B. Roles will change to magaox01 and magaox02 after provisioning
```
./magaox_multipass_setup.sh -v=magaox01 -2=magaox02 -M=drbitboy/MagAOX,MagAOX,resurrector-20250423 -r=drbitboy/magao-x-config,config,resurrector-indi-compression
```

## As user ubuntu on both magaox01 and magaox02:

### Cache config file that user xsup can use; switch to user xsup

```
resuctrl reset
sudo su - xsup
```

## As user xsup:

### Start INDI server and all INDI drivers under resurrector control

```
resuctrl startup
resuctrl status --all
```

### Stop an INDI driver

```
resuctrl stop maths_1
resuctrl status --all
```

### Start an INDI driver

```
resuctrl start maths_x
resuctrl status --all
```

### Stop everything

```
resuctrl stop --all
```
