# onomondo-ipa
onomondo-ipa is a C based IoT Profile Assistant in the IoT Device (IPAd, see also SGP.31) implementation. The IPAd is an
element in the 3GPP IoT eSIM system as described in SGP.31 and SGP.32. It interfaces between the eUICC on side, 
and the eIM (via HTTPS) on the other side. The implementation presented here can run on a regular Linux host. It can also be used
as a library to add IPAd functionality to an IoT device that runs an RTOS.

Interfaces
----------

### ESipa

The ESipa interface of onomondo-ipa is implemented as an HTTP client interface (see also GSMA SGP.32, section 6.1) that
uses ASN.1 function bindings (see also GSMA SGP.32, section 6.3).

### ES10x

The interface towards the eUICC is implemented according to GSMA SGP.32 and GSMA SGP.22. The ES10x interface of
onomondo-ipa features an IoT eUICC emulation mode. This allows the usage of regular consumer eUICCs, which are readily
available. The emulation replaces missing IoT eUICC functionality by calling an appropriate consumer eUICC function
as a replacement. In case no equivalent function is available, the function is emulated by onomondo-ipa directly. This
is in particular the case for the functions related to the management of the eIM configuration.

Installation
------------

### Dependencies

The IPAd core implementation (libasn, libipa) written in a way so that it has no dependencies other than a c99 compliant
C-compiler. However, onomondo-ipa still requires platform dependent modules that allow it to make HTTP(s) requests and
to access the eUICC via some sort of smart card reader. This repository ships with a sample implementation of those
platform-dependent modules that can run on a standard Linux system:

* `http.c`: Contains a libcurl based implementation to make HTTP(s) requests.
* `scard.c`: Contains a libpcsclite based implementation to access the eUICC.

On a Debian GNU/Linux system the following packages are required:

* `libcurl4-gnutls-dev`
* `libpcsclite-dev`
* `build-essential`
* `cmake`

On a Debian system, the standard `apt-get install ...` command can be used to install those dependencies.

### Building

Run the following steps to compile the IPAd:

```
cmake -S . -B build -DENABLE_SANITIZE=ON -DSHOW_ASN_OUTPUT=ON
cmake --build build
```

#### Options

* `-DENABLE_SANITIZE`
this feature is entirely optional and results in a build with AddressSanitizer, which helps to
find out-of-bounds memory accesses during development and testing.
* `-DSHOW_ASN_OUTPUT`
enables decoded printing of the ASN.1 encoded messages that are exchanged between eIM and eUICC.
The decoded ASN.1 output may result in large log output, so it is recommended to use this option only for
development/testing. (The hexadecimal representation of messages is still printed)
* `-DASN_EMIT_DEBUG`
the code that is used to encode/decode ASN.1 encoded messages has been generated using asn1c. This
ASN.1 compiler also adds debug messages, which can be enabled by adding this option.
* `-DMEM_EMIT_DEBUG`
this option can be used to analyze the usage of heap memory. When this option is enabled IPA_ALLOC,
IPA_ALLOC_N, IPA_REALLOC, and IPA_FREE will keep track on how much memory is currently allocated. The current memory
usage and the peak memory usage is then displayed. The feature relies on the function malloc_usable_size(), which is a
non standard API. However, the function is available on GNU LINUX and FreeBSD (see also man malloc_usable_size)
* `-DM32`
use this option to compile onomondo-ipa for 32-BIT x86 architectures,
see also GCC manual, section 3.19.54 x86 Options.


Usage
-----

Along with the platform depended modules, a sample application (`main.c`) is also included. This sample application is
a fully functional IPAd that runs on a linux system. However, its main purpose is to illustrate how to use the API
presented in `onomondo/ipad.h`.

### Commandline options

There are a number of commandline options supported. The most relevant options are:

* `-r` specifies the PCSC reader number.
* `-f` specifies the path to an initial eIM configuration file
* `-I` omit verification of the SSL certificate of the eIM. This option makes the operation of onomondo-ipa insecure,
but maybe helpful for testing and debugging in lab setups.
* `-E` enable the IoT eUICC emulation in case a regular consumer eUICC should be used.

(use option -h to query the full list of parameters)

### Initial setup

During the first run, onomondo-ipa will create an `nvstate.bin` file in its working directory. 
This file is used as non-volatile storage of data.

In case the IoT eUICC is not yet provisioned with an eIM configuration, onomondo-ipa can be used to perform the
provisioning. The configuration must be supplied as a file that contains an AddInitialEimRequest (see also GSMA SGP.32,
section 5.9.18) data object in its encoded form. It is up to the user to compile the data object using appropriate tools
and the ASN specification presented in GSMA SGP.32. For testing purposes onomondo-ipa ships with a sample configuration
(contrib/sample_eim_cfg.ber) that expects the eIM to be running at 127.0.0.1:8000.

Example: load the initial eIM configuration onto the eUICC in PCSC reader 2
```
./src/ipa/ipa -r 2 -f ../contrib/sample_eim_cfg.ber
```

### Quering eIM packages

When onomondo-ipa is called without the `-f` parameter, it will read the eUICC configuration and the eidValue from the
eUICC and use it to query the eIM for eIM packages. In case no eIM package is available (error code
noEimPackageAvailable), onomondo-ipa will exit. This condition is technically not an error, it just means that currently
no eIM package is avaliable for the given eUICC / eidValue.

When there is an eIM package available for the given eUICC / eidValue, then onomondo-ipa will download it and execute
the requested procedure. Immediately after that, the next eIM package is requested and processed until the eIM returns
the error code noEimPackageAvailable.

Example: Query the eIM for eIM packages
```
./src/ipa/ipa -r 2
```
