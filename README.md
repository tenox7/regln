
REGLN - Windows Rregistry Linking Utility, v2.2
===============================================
Copyright (c) 1999, Antoni Sawicki


License
-------
  Apache 2.0


Usage instructions
------------------
```
  Usage: regln [-v] <link_key>  <target_key>
         regln  -d  <link_key>

  <link_key> is the new registry link key
  <target_key> is an existing registry key being linked to
  -v = volatile, exist in memory only
  -d = delete link
```  

  Regln can either create or delete a registry link.

  To create a registry link you need to have an existing registry key to link 
  to and decide whenever the link has to be volatile or permanent.

  Volatile links will be stored in the RAM memory and will not be saved in any
  HIVE file. This is usefull for experimenting because a simple reboot will
  remove the link. If you don't specify "-v" option the link will be created
  permanently by default. This is potentially dangerous operation.

  The application uses NT Namespace Paths by default, eg:

```
  \Registry\Machine\Software\Microsoft
```

  However for compatibility and ease of use, a conversion function to translate
  to Win32 key names was implemented in the public release. From now on, you can
  simply use:

```
  HKEY_LOCAL_MACHINE\Software\Microsoft
```

  or:

```
  HKLM\Software\Microsoft
```

  Unfortunately the "virtual root keys" like HKEY_CURRENT_USER  and others are
  only Win32 addition and are not visible from the Native API and therefore
  it's not possible use them in REGLN. The only valid "root keys" are:

```
  Win32                    Abbr       Namespace
  -----------------------------------------------------
  HKEY_LOCAL_MACHINE       HKLM       \Registry\Machine
  HKEY_USERS               HKUS       \Registry\User
```

  Note: For compatibility with scripts and other utilities the root key name "HKU"
  was added as synonym of HKUS or HKEY_USERS.

  The keys: HKEY_CURRENT_USER, HKEY_DYN_DATA, HKEY_CLASSES_ROOT or any other CANNOT
  be used by REGLN. No, it is not possible at all.

  Also you cannot create links (or any other keys) directly under the "root keys",
  for example from one user profile (in HKUS) to another. Also you cannot link
  HKLM\FREEWARE to HKLM\SOFTWARE etc. If you want to do that,  you have to load up
  the hive file and link it's all top-level entries. Perhaps it would be possible
  to do that using Namespace Links, however it's out of scope of this utility.
  

  To Delete a registry link you simply have to specify the link-key name with "-d"
  option. It does not matter if the link is volatile or not.


Examples
--------
```
  regln -v HKLM\Software\TestInc  HKLM\Software\Microsoft
```

  Will create a temporary link TestInc pointing to Microsoft. If you open Regedit
  or any other registry editor and go to HKLM->Software and then TestInc you will
  see exacly same content as in Microsoft key. If you add/delete/change something 
  in the Microsoft tree, it will be obviously instantly "changed" in TestInc as
  well. (Remember to refresh display by pressing F5 if you're using the system
  regedit.exe)

```
  regln -d HKLM\Software\TestInc
```

  Will remove the link...

  Now a more advanced example:

  You can rename `HKLM\Software\Microsoft\Windows\CurrentVersion`
  to `HKLM\Software\Microsoft\Windows\Version1`  and create creatie
  a following link:

```
  regln \Registry\Machine\Software\Microsoft\Windows\CurrentVersion 
        \Registry\Machine\Software\Microsoft\Windows\Version1
```

  This will create a permanent link pointing from CurrentVersion to the existing
  key Version1. Now copy the whole tree recursively from Version1 to Version2 so
  that you'll have:

```
  HKLM\Software\Microsoft\Windows:

      - CurrentVersion --LINK-->> Version1
      - Version1
      - Version2
```

  Now, you change some variables in Version2 and try to re-link the key
  CurrentVersion to Version2 by executing:

```
  regln -d \Registry\Machine\Software\Microsoft\Windows\CurrentVersion 

  regln \Registry\Machine\Software\Microsoft\Windows\CurrentVersion 
        \Registry\Machine\Software\Microsoft\Windows\Version2
```

  As you can see, you can have several "sets of settings" for various purposes,
  and swap them around easily. It seems clear that the "CurrentVersion" key was
  really intended just for that, except it was never fully implemented until now.

  Note you have to extend Registry Quota to have enough space to do things like
  this. Also most applications will have open handles to the registry keys and
  will write to the old place so you usualy will have to reboot to use this. 
  However if the application uses registry dynamically (opens and closes keys),
  it will work just fine.



Real life use of Registry Links
-------------------------------
  Microsoft Windows uses Registry Links internally for the feature better known as
  "Last Good Known Configuration" to this day.
  
  Windows used to have concept of Hardware Profiles prior to Windows XP. The feature
  allowed to have some Windows settings changed at boot time by selecting one of the
  profiles. The amount of Hardware Profile dependent settings was rather limited.
  Regln allows to extend the functionality by making any Windows registry key to 
  be profile dependent.

  Regln has been used to allow to split various system level settings to be made
  user dependent by linking them from HKLM to HKUS. Similarly settings that are
  normally user dependent (HKUS) can be made system wide (HKLM) so that every user
  will have the same centrally managed settings. This has been extensively used
  in Terminal Server Edition deployments.

  Registry links have been used to link a group of disperse and unrelated settings
  to be stored in the same hive file.

  The application has been used to make several user profiles to link their settings
  to a single "master" user.

  More recently it was reported that Regln was used to solve a problem with daylight
  saving time and TZInfo over reboot of a EWF enabled device on Windows XP Embedded. 

Credits
-------    
    Antoni Sawicki <as@tenoware.com>
    Tomasz Nowak <tn@tenoware.com>
    Mark Russinovitch <mark@sysinternals.com>

    Special thanks to adder_2003@yahoo.com for recent code fixes
