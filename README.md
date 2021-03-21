DIRTSAND - The D'ni In Real-Time Server And Network Dæmon
=========================================================

Introduction
------------

DIRTSAND is a full featured MOULa-compatible server platform for POSIX-compliant
operating systems, written in C++ and released under the AGPL version 3+.
Currently, it has only been tested on Linux, but in theory it should work on
other Unixes as well.  There are, however, currently no plans for Windows
development or support.


Prerequisites
-------------
* GCC 4.7+ (might work with other C++11 compliant compilers, but untested)
* PostgreSQL 9.0+ (the database server can be run on another machine)
* libpq
* OpenSSL
* libreadline
* zlib
* [string_theory](https://github.com/zrax/string_theory)
* git (to get the sources)


Building the code
-----------------
1) Create a user and path for the DIRTSAND server to run from:

   ```
   $ sudo useradd dirtsand -d /opt/dirtsand -m -p <password> -s /bin/bash
   $ su dirtsand
   ```

2) Check out a copy of the source:

   ```
   $ git clone git://github.com/H-uru/dirtsand.git dirtsand
   $ cd dirtsand
   ```

3) Compile it for your system:

   ```
   $ mkdir build && cd build
   $ cmake -DCMAKE_INSTALL_PREFIX=/opt/dirtsand ..
   $ make && sudo make install
   $ cd ..
   ```

   If you run into any errors about finding libraries or headers, make sure
   you have the *development* versions of all of the required libraries, and
   that they are in your path.  You can also use the `cmake-gui` to help cmake
   locate the missing paths and files.


Setting up a server
-------------------

You will need a working Postgres server which DIRTSAND can use to store its
data.  Although you don't need superuser access to the postgres server, you
will need to have a database which you can add schemas, tables, etc into.

Setting up postgres functionality without a superuser account is currently
beyond the scope of this document...  If you need to do this, keep in mind
that you may also have to edit the database initialization scripts to work
against an existing user and/or database.

For the default installation, the provided scripts will create a dirtsand
database and set its ownership to a 'dirtsand' database user, which can
directly map the system dirtsand user created in step 1 of the "Building
the code" instructions above.  For better security, it is recommended to
use a password (as shown in the steps below), which can be configured in
the server settings as described in the "configure dirtsand" step.


1) Set up the postgres user:

   ```
   $ sudo -u postgres psql -d template1
   template1=# CREATE USER dirtsand WITH PASSWORD '<password>';
   template1=# CREATE DATABASE dirtsand WITH TEMPLATE = template0 ENCODING = 'UTF8';
   template1=# ALTER DATABASE dirtsand OWNER TO dirtsand;
   template1=# \q
   ```

2) Install the UUID functionality:

   This may be provided by your OS distribution.  In Ubuntu, simply install
   the `postgresql-contrib` package to provide the necessary libraries and
   installation scripts.  If your distribution does not provide a contrib
   or uuid-ossp bundle, you can get it and build it yourself from the
   sources provided at:  http://www.ossp.org/pkg/lib/uuid/

   In versions of Postgres **older than** 9.x, you will need to use the
   import script to add the uuid extension to the dirtsand database:

   ```
   $ sudo -u postgres psql -d dirtsand < /path/to/uuid-ossp.sql

   # (Example for Ubuntu 10.10 with PostgreSQL 8.4)
   $ sudo -u postgres psql -d dirtsand < /usr/share/postgresql/8.4/contrib/uuid-ossp.sql
   ```

   In PostgreSQL 9.x and newer, simply use the "CREATE EXTENSION" syntax to
   add the uuid-ossp extension it to the Dirtsand database:

   ```
   $ sudo -u postgres psql -d dirtsand
   dirtsand=# CREATE EXTENSION "uuid-ossp";
   dirtsand=# \q
   ```

3) Set up the dirtsand database:

   ```
   $ psql -d dirtsand < db/dbinit.sql
   $ psql -d dirtsand < db/functions.sql
   ```

   If there were no errors, your database should be ready for DIRTSAND.

4) Configure dirtsand:

   A sample dirtsand.ini has been provided in the root of the dirtsand
   sources.  We can copy this to our install directory and then edit the
   fields we need.  Specifically, you will need to adjust the server
   addresses and the RC4 keys.  If you have dirtsand installed to somewhere
   other than /opt/dirtsand, you will also need to point the configuration
   to the right paths too.

    ```
   $ sudo cp dirtsand.sample.ini /opt/dirtsand/dirtsand.ini
   $ sudo chown dirtsand /opt/dirtsand/dirtsand.ini
   $ <your-favorite-editor> dirtsand.ini
   ```

   To generate the RC4 keys, you can simply run the command-line option
   `generate-keys` using dirtsand:

   ```
   $ bin/dirtsand --generate-keys
   ```

   You should now have a bunch of keys output on your terminal.  The first
   block (labeled Server keys) is the set you should paste into your
   dirtsand.ini.  Replace the dummy lines (with the '...' values) with the
   output from the `generate-keys` command.  The second set of keys can be
   placed directly in the client's server.ini file (NOTE: This requires
   either the H-uru fork of CWE or PlasmaClient -- the vanilla MOUL
   client cannot load keys from a file, and you will have to enter the
   keys as byte arrays directly into the executable.

5) Provide data for the client to use:

   The most important data for the client are the auth server provided
   files -- specifically, the SDL and python.pak.  External Plasma clients
   require these files in order to function, so you will need to provide
   them unless you are planning on making your server only work with
   Internal client builds.

   Generating or acquiring these files is currently outside the scope of
   this document, but you can find more information on that process at the
   [moul-scripts github project page](http://github.com/H-uru/moul-scripts).

   To provide the files to the client, set up a directory for auth data
   (the default is /opt/dirtsand/authdata) and put the files in their
   respective subdirectories (SDL/\*.sdl and Python/\*.pak).  Note that
   these files will need to be NTD encrypted with the key specified in
   your dirtsand.ini file (the default is the first 32 decimal digits of
   pi, without the decimal point).  You can specify any 32 hex digit key,
   but make sure to update the files to match.

   In order to let the client know what files are available, you will
   also need to provide the files Python_pak.list and SDL_sdl.list, which
   are simple manifest files of the format:

   ```
   Path\filename.ext,size-in-bytes
   ```

   So, for python.pak, you might have:

   ```
   Python\python.pak,123456
   ```

   Note the use of a backslash here, since this path is provided directly
   to the client (which assumes a Windows path).  Comments (starting with
   the `#` character) and whitespace will be ignored when parsing this file.

   --------

   DIRTSAND also includes a full data server with its services.  If you
   choose to use it, you will need to set up a data/ directory and the
   necessary manifests.  At the very least, you will need the
   ThinExternal.mfs and External.mfs files, which describe the files
   necessary for the patcher and client (respectively) to run.  You may
   also provide any number of age manifests, which will be requested by
   the client when it attempts to link to an age (e.g., Ahnonay.mfs).

   The format of the manifest files is as follows:

   ```
   remote_filename.ext,local_filename.ext.gz,decompressed_hash,compressed_hash,decompressed_size,compressed_size,flags
   ```

   There is also a helper script provided in bin/dsData.sh which will gzip
   a file and generate a manifest line with the correct hashes and sizes
   for you automatically.  Take care that the remote path expects to use a
   Windows path/filename, so it should use backslashes instead of forward
   ones, whereas the local filename should use Unix slashes.

   Alternatively, you can specify a remote file server address in your
   dirtsand.ini, which will allow the files to be fetched from an external
   file server.

6) Run the server:

   Assuming everything else went smoothly, you should now be able to start
   your server and connect to it!  You'll have to create an account first,
   which can be done from the console:

   ```
   $ bin/dirtsand dirtsand.ini
   ds> addacct <username> <password>
   ```

   If you can't connect for some reason, make sure you copied the keys
   and server addresses correctly into the client's server.ini, and check
   that your firewall is set to allow connections from port 14617.

   If you want to leave the server running across different login sessions
   and you don't have an X or VNC server running, I recommend running
   dirtsand in a detachable GNU screen session.


Additional Information
----------------------

If you have bugs to report, or you wish to submit code to improve DIRTSAND,
you can visit the [github project page](http://github.com/H-uru/dirtsand).

To get the H-uru fork of the client, which has the best support for DIRTSAND
servers, visit the [H-uru/Plasma project](http://github.com/H-uru/Plasma).
