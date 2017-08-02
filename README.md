# Android<sup>tm*</sup> Auto interoperability library

In its current state this project can only act as a headunit, but I plan to implement the phone side + some other modes as well.

The library is based on, and makes extensive use of the following projects:
 
 - JNI functions from [mikereidis/headunit](https://github.com/mikereidis/headunit)
 - The protocol handler and the USB/TCP specific code from (the plaform independent bits) [gartnera/headunit](https://github.com/gartnera/headunit), which is based on mikreidis' work, first forked by [konsulko](https://github.com/konsulko/headunit) then [spadival](https://github.com/spadival/headunit).

This library by itself is based on libusb, OpenSSL, Protobuf and some Linux specific libraries (libudev and libunwind)

-----------------
*Android is a trademark of Google Inc.*
