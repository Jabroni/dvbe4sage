# Introduction #

DVBE4SAGE can be run in service mode, i.e. with no GUI. The main advantage of running it in service mode is that the service is started automatically upon computer reboot, even if nobody is logged in. This is especially important if the computer is set to receive **and install** system updates automatically (Windows default), which might cause your recording server to reboot in the middle of the night.

SageTV service is made dependent upon DVBE4SAGE service(s), so its initialization is started only **after** all DVBE4SAGE services it depends upon finished their initialization procedures. This is very important, since DVBE4SAGE initialization might take time (up to 1.5 minutes in some cases).


# Details #

Each instance of DVBE4SAGE service needs to be installed using command shell (CMD). It needs to be started with administrative privileges ("Run as Administrator" in Vista and 7).

The following command installs DVBE4SAGE service:

`dvbe4sagesvc /install [/servicename:`_`name`_`] [/user:`_`domain\username`_`] [/password:`_`password`_`]`

All the flags but **/install** are optional. When **/servicename** is not specified, the default name is assigned (_"DVB Enhancer for SageTV"_). When **/user** is not specified, the service is installed as _**LocalSystem**_. When **/user** is specified but **/password** is not, the user is prompted for the password. If the user name is that of a local user (**not** of a domain user), it should be specified as **.\username**.

The following command uninstalls DVBE4SAGE service:

`dvbe4sagesvc /uninstall [/servicename:`_`name`_`]`

Note, that in some cases the uninstallation is completed successfully, but the service is not immediately deleted from the system. In this case it will be deleted after system reboot.

### Note on multiple instance installations ###

It is possible to install multiple service instances using the same set of executable files. For this, **dvbe4sagesvc /install** needs to be invoked from the directory where the instance-specific files (**dvbe4sage.ini** file and **Plugins** directory) are located.

Example:

_DVBE4SAGE root directory_
```
dvbe4sage.exe
dvbe4sagesvc.exe
ecmcache.xsd
encoder.dll
ProvX                     <dir>
ProvY                     <dir>
```
_Service #1 (Provider X) directory_
```
dvbe4sage.ini
Plugins                   <dir>
```
_Service #2 (Provider Y) directory_
```
dvbe4sage.ini
Plugins                   <dir>
```
Installing service #1:
```
C:\DVBE4SAGE\ProvX>..\dvbe4sagesvc.exe /install /servicename:"DVB Enhancer for SageTV for ProvX"
Service successfully installed!
```
Installing service #2:
```
C:\DVBE4SAGE\ProvY>..\dvbe4sagesvc.exe /install /servicename:"DVB Enhancer for SageTV for ProvY"
Service successfully installed!
```