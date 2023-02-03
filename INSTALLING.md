# Installation
As mentioned in `README.md` the `vmbsrc` element is contained in a single file. In order for
GStreamer to use the element, the shared library containing `vmbsrc` must be findable by GStreamer.
This can be achieved by defining the `GST_PLUGIN_SYSTEM_PATH` and placing the shared library file in
that directory. Additionally, the VmbC shared library (and its dependencies) must be loadable as it
is used by the `vmbsrc` element. VmbC is provided as part of the Vimba X SDK.

Below are more details on the installation on Linux (more specifically Ubuntu) and Windows.

## Linux (Ubuntu)
If the `GST_PLUGIN_SYSTEM_PATH` variable is not defined, the default paths of the system wide
GStreamer installation, as well as the `~/.local/share/gstreamer-1.0/plugins` directory of the
current user are searched. Installing the `vmbsrc` element is therefore simply a matter of placing
the compiled shared library `libgstvmbsrc.so` into this search path and letting GStreamer load it.

### Installation dependencies
As the shared library containing the `vmbsrc` element is dynamically linked, its linked dependencies
must be loadable. As GStreamer itself is likely installed system wide, the dependencies on glib and
GStreamer libraries should already be satisfied.

In order to satisfy the dependency on VmbC, the shared library `libVmbC.so` needs to be loadable at
runtime. The easiest way to achieve this is to add a file to `/etc/ld.so.conf.d/` which contains the
path to `/install/path/on/your/system/to/VimbaX/api/lib`.

After creating that file the `ldconfig` cache needs to be updated. This can be done by running the
command `sudo ldconfig -v`.

Correct installation of `libVmbC.so` can be checked by searching for its file name in the output of
`ldconfig -v` (e.g.: `ldconfig -v | grep libVmbC.so`). Alternatively, correct loading of dependent
shared libraries can be checked with `ldd` (e.g. `ldd libgstvmbsrc.so`).

## Windows
Adding the directory containing the compiled shared library file to the `GST_PLUGIN_SYSTEM_PATH` is
the easiest way to install the `vmbsrc` element. Alternatively, the file can be placed in the
GStreamer installation directory of the system. The installation directory was chosen during the
installation of the GStreamer runtime. The directory the plugin should be placed into is
`<GSTREAMER_INSTALLATION_DIRECTORY>\1.0\msvc_x86_64\lib\gstreamer-1.0` on 64-bit systems.

### Installation dependencies
In addition to the installation of GStreamer runtime and placing the `vmbsrc` shared library into
the GStreamer plugin search path, VmbC needs to be loadable. This can be achieved by adding a
directory containing `VmbC.dll` to your systems `PATH` variable. With a working Vimba X installation
the directory `%VIMBA_X_HOME%\api\bin` will contain `VmbC.dll` and its dependencies.

## Further information
More information on the environment variables used by GStreamer to determine directories which
should be searched for elements can be found in [the official
documentation](https://gstreamer.freedesktop.org/documentation/gstreamer/running.html).