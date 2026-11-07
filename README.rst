aria2 - The ultra fast download utility
=======================================

Disclaimer
----------
This program comes with no warranty.
You must use this program at your own risk.

Introduction
------------

aria2 is a utility for downloading files. The supported protocols are
HTTP(S), FTP, SFTP and BitTorrent. aria2 can download a
file from multiple sources/protocols and tries to utilize your maximum
download bandwidth. It supports downloading a file from
HTTP(S)/FTP/SFTP and BitTorrent at the same time, while the data
downloaded from HTTP(S)/FTP/SFTP is uploaded to the BitTorrent
swarm.

The project page is located at https://aria2.github.io/.

See the `aria2 Online Manual
<https://aria2.github.io/manual/en/html/>`_ to learn how to use aria2.

Features
--------

Here is a list of features:

* Command-line interface
* Download files through HTTP(S)/FTP/SFTP/BitTorrent
* Segmented downloading
* HTTP/1.1 implementation
* HTTP Proxy support
* HTTP BASIC authentication support
* HTTP Proxy authentication support
* Well-known environment variables for proxy: ``http_proxy``,
  ``https_proxy``, ``ftp_proxy``, ``all_proxy`` and ``no_proxy``
* HTTP gzip, deflate content encoding support
* Verify peer using given trusted CA certificate in HTTPS
* Client certificate authentication in HTTPS
* Chunked transfer encoding support
* Load Cookies from the file using the Firefox3 format, Chromium/Google Chrome
  and the Mozilla/Firefox
  (1.x/2.x)/Netscape format.
* Save Cookies in the Mozilla/Firefox (1.x/2.x)/Netscape format.
* Custom HTTP Header support
* Persistent Connections support
* FTP/SFTP through HTTP Proxy
* Download/Upload speed throttling
* BitTorrent extensions: Fast extension, DHT, PEX, MSE/PSE,
  Multi-Tracker, UDP tracker
* BitTorrent `WEB-Seeding <http://getright.com/seedtorrent.html>`_.
  aria2 requests chunk more than piece size to reduce the request
  overhead. It also supports pipelined requests with piece size.
* BitTorrent Local Peer Discovery
* Rename/change the directory structure of BitTorrent downloads
  completely
* JSON-RPC (over HTTP and WebSocket) interface
* Run as a daemon process
* Netrc support
* Configuration file support
* Download URIs found in a text file or stdin and the destination
  directory and output file name can be specified optionally
* Parameterized URI support
* IPv6 support with Happy Eyeballs
* Disk cache to reduce disk activity


Versioning and release schedule
-------------------------------

We use 3 numbers for the aria2 version: MAJOR.MINOR.PATCH.  We will ship
MINOR updates on the 15th of every month.  We may skip a release if we have
had no changes since the last release.  The feature and documentation
freeze happens 10 days before the release day (the 5th day of the month)
for translation teams.  We will raise an issue about the upcoming
release around that day.

We may release PATCH releases between regular releases if we have
security issues.

The MAJOR version will stay at 1 for the time being.

How to get source code
----------------------

We maintain the source code at Github:
https://github.com/aria2/aria2

To get the latest source code, run the following command::

    $ git clone https://github.com/aria2/aria2.git

This will create an aria2 directory in your current directory and source
files are stored there.

Dependency
----------


======================== ========================================
features                  dependency
======================== ========================================
HTTPS                    GnuTLS or OpenSSL
SFTP                     libssh2
BitTorrent               None. Optional: libnettle+libgmp or libgcrypt
                         or OpenSSL (see note)
Checksum                 None. Optional: libnettle or libgcrypt
                         or OpenSSL (see note)
gzip, deflate in HTTP    zlib
Async DNS                C-Ares
Firefox3/Chromium cookie libsqlite3
JSON-RPC over WebSocket  libnettle or libgcrypt or OpenSSL
======================== ========================================


.. note::

  GnuTLS has precedence over OpenSSL if both libraries are installed.
  If you prefer OpenSSL, run configure with ``--without-gnutls``
  ``--with-openssl``.

.. note::

  libnettle has precedence over libgcrypt if both libraries are
  installed.  If you prefer libgcrypt, run configure with
  ``--without-libnettle --with-libgcrypt``. If OpenSSL is selected over
  GnuTLS, neither libnettle nor libgcrypt will be used.

  If none of the optional dependencies are installed, an internal
  implementation that only supports md5 and sha1 will be used.

A user can have one of the following configurations for SSL and crypto
libraries:

* OpenSSL
* GnuTLS + libgcrypt
* GnuTLS + libnettle

You can disable BitTorrent support by providing
``--disable-bittorrent`` to the configure script.

To enable async DNS support, you need c-ares.

* c-ares: http://c-ares.haxx.se/

How to build
------------

aria2 is primarily written in C++. Initially, it was written based on
C++98/C++03 standard features. We are now migrating aria2 to the C++11
standard. The current source code requires a C++11 aware compiler. For
well-known compilers, such as g++ and clang, the ``-std=c++11`` or
``-std=c++0x`` flag must be supported.

To build aria2 from the source package, you need the following
development packages (package name may vary depending on the
distribution you use):

* libgnutls-dev    (Required for HTTPS, BitTorrent, Checksum support)
* nettle-dev       (Required for BitTorrent, Checksum support)
* libgmp-dev       (Required for BitTorrent)
* libssh2-1-dev    (Required for SFTP support)
* libc-ares-dev    (Required for async DNS support)
* zlib1g-dev       (Required for gzip, deflate decoding support in HTTP)
* libsqlite3-dev   (Required for Firefox3/Chromium cookie support)
* pkg-config       (Required to detect installed libraries)

You can use libgcrypt-dev instead of nettle-dev and libgmp-dev:

* libgpg-error-dev (Required for BitTorrent, Checksum support)
* libgcrypt-dev    (Required for BitTorrent, Checksum support)

You can use libssl-dev instead of
libgnutls-dev, nettle-dev, libgmp-dev, libgpg-error-dev and libgcrypt-dev:

* libssl-dev       (Required for HTTPS, BitTorrent, Checksum support)

On Fedora you need the following packages: gcc, gcc-c++, kernel-devel,
libgcrypt-devel, openssl-devel, cppunit

If you downloaded source code from a git repository, you have to install
the following packages to get autoconf macros:

* libcppunit-dev
* autoconf
* automake
* autotools-dev
* libtool

And run the following command to generate configure script and other files
necessary to build the program::

    $ autoreconf -i

Also, you need `Sphinx <http://sphinx-doc.org/>`_ to build the man page.

The quickest way to build aria2 is first to run configure script::

    $ ./configure

To build statically linked aria2, use ``ARIA2_STATIC=yes``
command-line option::

    $ ./configure ARIA2_STATIC=yes

After configuration is done, run ``make`` to compile the program::

    $ make

The configure script checks available libraries and enables as many
features as possible except for experimental features not enabled by
default.

Since 1.1.0, aria2 checks the certificate of HTTPS servers by default.
If you build with OpenSSL or the recent version of GnuTLS which has
``gnutls_certificate_set_x509_system_trust()`` function and the
library is properly configured to locate the system-wide CA
certificates store, aria2 will automatically load those certificates
at the startup. If it is not the case, I recommend supplying the path
to the CA bundle file. For example, in Debian the path to CA bundle
file is '/etc/ssl/certs/ca-certificates.crt' (in ca-certificates
package). This may vary depending on your distribution. You can give
it to configure script using ``--with-ca-bundle option``::

    $ ./configure --with-ca-bundle='/etc/ssl/certs/ca-certificates.crt'
    $ make

Without ``--with-ca-bundle`` option, you will encounter the error when
accessing HTTPS servers because the certificate cannot be verified
without the CA bundle. In such a case, you can specify the CA bundle file
using aria2's ``--ca-certificate`` option.  If you don't have the CA bundle
file installed, then the last resort is to disable the certificate
validation using ``--check-certificate=false``.

By default, the bash_completion file named ``aria2c`` is installed to
the directory ``$prefix/share/doc/aria2/bash_completion``.  To change
the install directory of the file, use ``--with-bashcompletiondir``
option.

After a ``make``, the executable is located at ``src/aria2c``.

aria2 uses CppUnit for automated unit testing. To run the unit test::

    $ make check

Building documentation
----------------------

`Sphinx <http://sphinx-doc.org/>`_ is used to building the
documentation. aria2 man pages will be build when you run ``make`` if
they are not up-to-date.  You can also build an HTML version of the aria2
man page by ``make html``. The HTML version manual is also available
`online <https://aria2.github.io/manual/en/html/>`_.

BitTorrent
-----------

About file names
~~~~~~~~~~~~~~~~
The file name of the downloaded file is determined as follows:

single-file mode
    If "name" key is present in .torrent file, the file name is the value
    of "name" key. Otherwise, the file name is the base name of .torrent
    file appended by ".file". For example, .torrent file is
    "test.torrent", then file name is "test.torrent.file".  The
    directory to store the downloaded file can be specified by -d
    option.

multi-file mode
    The complete directory/file structure mentioned in .torrent file
    is created.  The directory to store the top directory of
    downloaded files can be specified by -d option.

Before download starts, a complete directory structure is created if
needed. By default, aria2 opens at most 100 files mentioned in
.torrent file, and directly writes to and reads from these files.
The number of files to open simultaneously can be controlled by
``--bt-max-open-files`` option.

DHT
~~~

aria2 supports mainline compatible DHT. By default, the routing table
for IPv4 DHT is saved to ``$XDG_CACHE_HOME/aria2/dht.dat`` and the
routing table for IPv6 DHT is saved to
``$XDG_CACHE_HOME/aria2/dht6.dat`` unless files exist at
``$HOME/.aria2/dht.dat`` or ``$HOME/.aria2/dht6.dat``. aria2 uses the
same port number to listen on for both IPv4 and IPv6 DHT.

UDP tracker
~~~~~~~~~~~

UDP tracker support is enabled when IPv4 DHT is enabled.  The port
number of the UDP tracker is shared with DHT. Use ``--dht-listen-port``
option to change the port number.

Other things should be noted
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

* ``-o`` option is used to change the file name of .torrent file itself,
  not a file name of a file in .torrent file. For this purpose, use
  ``--index-out`` option instead.
* The port numbers that aria2 uses by default are 6881-6999 for TCP
  and UDP.
* aria2 doesn't configure port-forwarding automatically. Please
  configure your router or firewall manually.
* The maximum number of peers is 55. This limit may be exceeded when
  the download rate is low. This download rate can be adjusted using
  ``--bt-request-peer-speed-limit`` option.
* As of release 0.10.0, aria2 stops sending request messages after
  selective download completes.

netrc
-----

netrc support is enabled by default for HTTP(S)/FTP/SFTP.  To disable
netrc support, specify -n command-line option.  Your .netrc file
should have correct permissions(600).

WebSocket
---------

The WebSocket server embedded in aria2 implements the specification
defined in RFC 6455. The supported protocol version is 13.

libaria2
--------

The libaria2 is a C++ library that offers aria2 functionality to the
client code. Currently, libaria2 is not built by default. To enable
libaria2, use ``--enable-libaria2`` configure option.  By default,
only the shared library is built. To build a static library, use
``--enable-static`` configure option as well. See libaria2
documentation to know how to use API.

References
----------

* `aria2 Online Manual <https://aria2.github.io/manual/en/html/>`_
* https://aria2.github.io/
* `RFC 959 FILE TRANSFER PROTOCOL (FTP) <http://tools.ietf.org/html/rfc959>`_
* `RFC 1738 Uniform Resource Locators (URL) <http://tools.ietf.org/html/rfc1738>`_
* `RFC 2428 FTP Extensions for IPv6 and NATs <http://tools.ietf.org/html/rfc2428>`_
* `RFC 2616 Hypertext Transfer Protocol -- HTTP/1.1 <http://tools.ietf.org/html/rfc2616>`_
* `RFC 3659 Extensions to FTP <http://tools.ietf.org/html/rfc3659>`_
* `RFC 3986 Uniform Resource Identifier (URI): Generic Syntax <http://tools.ietf.org/html/rfc3986>`_
* `RFC 4038 Application Aspects of IPv6 Transition <http://tools.ietf.org/html/rfc4038>`_
* `RFC 6265 HTTP State Management Mechanism <http://tools.ietf.org/html/rfc6265>`_
* `RFC 6266 Use of the Content-Disposition Header Field in the Hypertext Transfer Protocol (HTTP) <http://tools.ietf.org/html/rfc6266>`_
* `RFC 6455 The WebSocket Protocol <http://tools.ietf.org/html/rfc6455>`_
* `RFC 6555 Happy Eyeballs: Success with Dual-Stack Hosts <http://tools.ietf.org/html/rfc6555>`_

* `The BitTorrent Protocol Specification <http://www.bittorrent.org/beps/bep_0003.html>`_
* `BitTorrent: DHT Protocol <http://www.bittorrent.org/beps/bep_0005.html>`_
* `BitTorrent: Fast Extension <http://www.bittorrent.org/beps/bep_0006.html>`_
* `BitTorrent: IPv6 Tracker Extension <http://www.bittorrent.org/beps/bep_0007.html>`_
* `BitTorrent: Extension for Peers to Send Metadata Files <http://www.bittorrent.org/beps/bep_0009.html>`_
* `BitTorrent: Extension Protocol <http://www.bittorrent.org/beps/bep_0010.html>`_
* `BitTorrent: Multitracker Metadata Extension <http://www.bittorrent.org/beps/bep_0012.html>`_
* `BitTorrent: UDP Tracker Protocol for BitTorrent <http://www.bittorrent.org/beps/bep_0015.html>`_
  and `BitTorrent udp-tracker protocol specification <http://www.rasterbar.com/products/libtorrent/udp_tracker_protocol.html>`_.
* `BitTorrent: WebSeed - HTTP/FTP Seeding (GetRight style) <http://www.bittorrent.org/beps/bep_0019.html>`_
* `BitTorrent: Private Torrents <http://www.bittorrent.org/beps/bep_0027.html>`_
* `BitTorrent: BitTorrent DHT Extensions for IPv6 <http://www.bittorrent.org/beps/bep_0032.html>`_
* `BitTorrent: Message Stream Encryption <http://wiki.vuze.com/w/Message_Stream_Encryption>`_
* `Kademlia: A Peer-to-peer Information System Based on the  XOR Metric <https://pdos.csail.mit.edu/~petar/papers/maymounkov-kademlia-lncs.pdf>`_
