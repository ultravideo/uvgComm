## uvgComm Instructions

This tutorial assumes you have a built uvgComm and are ready to use it. Build instructions are found [here](BUILDING.md). This tutorial also assumes you have set up a SIP server such as Kamailio. It is also possible to have limited calling without a SIP server, but that aspect is not covered in this tutorial.

When you start uvgComm, you will be greeted by a window asking whether you want to enable STUN(Session Traversal Utilities for NAT). STUN is needed for media connectivity when you are behind a NAT.

Next, you should fill your user info for the server. You are free to choose your display name, but the username should exist on the server. Next, write your servers IP address (DNS lookup is not yet supported) and select the option that this is a SIP server. Write your password and lastly click `Save`. If everything went correctly, it should show your status as `Registered`. You are now free to make calls with anyone who has also registered to their SIP server. You may close the settings window after you have selected suitable audio/video device and screen from dropdown menus.

To call someone, you need to first add them as a contact. Click on the add contact button in the lower left corner and fill in the details. They should appear in you contact list and you may start a call with them by clicking the green call symbol. Ending the call happens from the red end call button at the bottom of the window.

### Further options

uvgComm offer plenty of user control for various aspects of audio and video processing. These options are hidden behind the show tab in settings view under `Advanced Settings`. Clicking the checkbox reveals button for detailed audio and video options intended for experts.

### Statistics

uvgComm also features a lightweight statistics collection for various aspects of the software. You can open the statistics view from `menu` -> `Statistics`. This view holds the SIP log, Call Parameters, Delivery statistics, Filter Graph Statistics and Performance Statistics separated in their own tabs.

### Architectures

In Call Settings, you can select the uvgComm instance to be either client or a server. You can also select which multi-party architecture uvgComm uses (if any). Only server/host architecture matters, clients will always follow the architecture set by host. In P2P Mesh its possible that the host also participants in the call as a client. Currently there is a bug that if you change the archtitecture, you may need to restart uvgComm for the change to take full effect. 