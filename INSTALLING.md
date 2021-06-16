# Installation
As mentioned in `README.md` the installation of `vimbasrc` consists of just two necessary files that
need to be placed in appropriate directories. The shared library containing the GStreamer element
must be findable by GStreamer. This can be achieved by defining the `GST_PLUGIN_SYSTEM_PATH` and
placing the shared library file in that directory. Additionally the VimbaC shared library must be
loadable as it is used by the `vimbasrc` element. VimbaC is provided as part of the Vimba SDK.

Below are more details on the installation on Linux (more specifically Ubuntu) and Windows.

## Linux (Ubuntu)
If the `GST_PLUGIN_SYSTEM_PATH` variable is not defined, the default paths of the system wide
GStreamer installation, as well as the `~/.local/share/gstreamer-<GST_API_VERSION>/plugins`
directory of the current user are searched. Installing the `vimbasrc` element is therefore simply a
matter of placing the compiled shared library into this search path and letting GStreamer load it.

### Installation dependencies
As the shared library containing the `vimbasrc` element is dynamically linked, its linked
dependencies must be loadable. As GStreamer itself is likely installed system wide, the dependencies
on glib and GStreamer libraries should already be satisfied.

In order to satisfy the dependency on `libVimbaC.so` the shared library needs to be placed in an
appropriate entry of the `LD_LIBRARY_PATH`. The exact method for this is a matter of preference and
distribution dependant. On Ubuntu systems one option would be to copy `libVimbaC.so` into
`/usr/local/lib` or to add the directory containing `libVimbaC.so` to the `LD_LIBRARY_PATH` by
adding an appropriate `.conf` file to `/etc/ld.so.conf.d/`.

Correct installation of `libVimbaC.so` can be checked, by searching for its file name in the output
of `ldconfig -v` (e.g.: `ldconfig -v | grep libVimbaC.so`). Alternatively correct loading of
dependent shared libraries can be checked with `ldd` (e.g. `ldd libgstvimbasrc.so`).

## Windows
Adding the directory containing the compiled shared library file to the `GST_PLUGIN_SYSTEM_PATH` is
the easiest way to install the `vimbasrc` element. Alternatively the file can be placed in the
GStreamer installation directory of the system. The installation directory was chosen during the
installation of the GStreamer runtime. The directory the plugin should be placed into is
`<GSTREAMER_INSTALLATION_DIRECTORY>\1.0\msvc_x86_64\lib\gstreamer-1.0` on 64 bit systems.

### Installation dependencies
In addition to the installation of GStreamer runtime and placing the `vimbasrc` shared library into
the GStreamer plugin search path, VimbaC needs to be loadable. This can be achieved by adding a
directory containing `VimbaC.dll` to your systems `PATH` variable. With a working Vimba installation
the directory `%VIMBA_HOME%VimbaC\Bin\Win64` will contain `VimbaC.dll` for 64 bit systems.

## Further information
More information on the environment variables used by GStreamer to determine directories which
should be searched for elements can be found in [the official
documentation](https://gstreamer.freedesktop.org/documentation/gstreamer/running.html).