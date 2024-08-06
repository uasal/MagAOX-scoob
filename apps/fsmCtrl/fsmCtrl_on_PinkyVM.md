### Steps to run fsmCtrl app on Pinky

#### Setup
1. In ~5 terminals:
    - ssh into Pinky as xsup (need to first have added you public key in /home/xsup/.ssh/authorized_keys)
    ```
    ssh xsup@10.130.133.96
    ```

    - Connect to brave-tattler VM. xsup has been authenticated on the VM, so it should not reprompt for that, but if it does, the password is the typical xsup password.
    ```
    multipass shell brave-tattler
    ```

3. In one terminal, start the trippLitePDU simulator:
```
cd /opt/MagAOX/source/MagAOX/apps/trippLitePDU
./trippLitePDU -n pdu_sim
```

4. In one terminal, start the xindiserver:
```
cd /opt/MagAOX/source/MagAOX/apps/xindiserver
./xindiserver --local.drivers=pdu_sim,fsmCtrl
```

5. In one terminal, open the fsmCtrl logs (could also open the telemetry in a different terminal):
```
cd /opt/MagAOX/logs
logdump -f fsmCtrl
```

6. In one terminal, start the fsmCtrl app:
```
cd /opt/MagAOX/source/MagAOX/apps/fsmCtrl
ssh -L 1337:localhost:1337 -N -f fsm
./fsmCtrl
```

7. In one last terminal, you can see the values for the INDI parameters with:
```
getINDI fsmCtrl
```

8. When the app starts, it will have the virtual power off, so need to turn that on:
```
setINDI -x pdu_sim.fsmCtrl.state=On
```


#### Drive fsmCtrl
- Editable parameters:
    - `fsmCtrl.input.toggle`

        Default value: `indi` (set in fsmCtrl.config)
    
        Possible values: `indi` / `shmim`
    - `fsmCtrl.input.type`
    
        Default value: `dacs` (set in fsmCtrl.config)
    
        Possible values: `dacs` / `voltages` / `angles`
    - `fsmCtrl.val_1.target`, `fsmCtrl.val_2.target`, `fsmCtrl.val_3.target`

        Comments: They only respond to new values if `fsmCtrl.input.toggle=indi`. They take values as indicated by `fsmCtrl.input.type` and within the range of the respective `fsmCtrl.dac_x.min` and `fsmCtrl.dac_x.max`.
    - `fsmCtrl.dac_1.min`, `fsmCtrl.dac_1.max`, `fsmCtrl.dac_2.min`, `fsmCtrl.dac_2.max`, `fsmCtrl.dac_3.min`, `fsmCtrl.dac_3.max`

        Comments: Range of accepted dac values for each actuator. It only takes dac values.
    - `fsmCtrl.conversion_factors.a`, `fsmCtrl.conversion_factors.b`

        Default value: a=0.0104; b=0.012

        Comments: Conversion factors for angles to actuator displacements (as used in [fsm_conversion_factors.ipynb](https://github.com/stefi07/MagAOX/blob/fsm/apps/fsmCtrl/fsm_conversion_factors.ipynb)). Angle conversion not currently finished.
    
    - `fsmCtrl.conversion_factors.v`

        Default value: v=1.46484375e-05 (from (4.096 / (2.0**24)) * 60)

        Comments: Conversion factor for dacs to voltages & back (as used at bottom of [FSMComm](https://gitlab.sc.ascendingnode.tech/pearl-inst-design/electronics/software/-/tree/master/AOApps/FineSteeringMirrorController/FSMComm.py)).

- Command via INDI params:
    To send voltages, for example:
    - Change input type to `voltages`:
    ```
    setINDI -n "fsmCtrl.input.type=voltages"
    ```

    - To send a voltage to actuator 3, for example:
    ```
    setINDI -n "fsmCtrl.val_3.target=80"
    ```

    - Check values (might need to give it a second after sending the voltage):
    ```
    getINDI fsmCtrl
    ```

- Command via shmim:
    The app is looking for a 1x3 shmim called `fsm` in the folder `/milk/shm/`. The shmim should also have a keyword with name `inputType` and value either `dacs`, `voltages` or `angles`.
    
    - To make new shmim in `milk`:
    ```
    $ milk
    milk > mk2Dim "s>fsm" 1 3
    milk > imkwaddS fsm inputType voltages comment
    ```
    (The first command makes a shmim called `fsm` of size 1x3 and saves it. The second command adds a keyword `inputType` with value `voltages` and comment `comment` to shmim `fsm`. The keyword requires an input for the `comment` field, but the fsmCtrl app only uses the value field.)

    Can also delete shmim with:
    ```
    > rmshmim fsm
    ```

    - To edit shmim values in ipython:
    ```
    $ ipython
    import numpy as np
    from magpyx.utils import ImageStream
    dm = ImageStream('fsm') # Looks for shmim `fsm` in /milk/shm/
    new = np.array([[30, 40, 80]]) # Make np array with new voltage values
    dm.write(new.transpose()) # Write new values to shmim
    ```

    - Other useful ipython commands:
    ```
    dm.grab_latest() # Check current shmim values
    dm.get_kws() # Check shmim's keywords
    ```