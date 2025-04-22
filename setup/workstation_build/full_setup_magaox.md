## As some user on multipass host OS:

```./magaox_multipass_setup.sh -v=magaox01 -M=drbitboy/MagAOX,MagAOX,resurrector-merge-dev-20250328-02plus-improve-resurrector -r=drbitboy/magao-x-config,config,resurrector-indi-compression
```

## As user ubuntu:

### Cache config file that user xsup can use; switch to user xsup

```resuctrl reset

sudo su - xsup```

## As user xsup:

### Start INDI server and all INDI drivers under resurrector control

```resuctrl startup
resuctrl status --all```

### Stop an INDI driver

```resuctrl stop maths_1
resuctrl status --all```

### Start an INDI driver

```resuctrl start maths_x
resuctrl status --all```

### Stop everything

```resuctrl stop --all```
