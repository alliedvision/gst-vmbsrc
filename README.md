# gst-vimbasrc
This project contains the official plugin to make cameras supported by Allied Vision Technologies
Vimba API available as GStreamer sources.

## Building
A CMakeLists.txt file is provided that should be used to build the plugin. For convenience this
repository also contains two scripts (`build.sh` and `build.bat`) that run the appropriate cmake
commands for Linux and Windows systems respectively. They will create a directory named `build` in
which the project files, as well as the built binary, will be placed.

As the build process relies on external libraries (such as GStreamer and Vimba), paths to these
libraries have to be detected. The provided build scripts take guesses (where possible) to find
these directories.

The Vimba installation directory is assumed to be defined by the `VIMBA_HOME` environment variable.
This is the case for Windows systems, on Linux systems you may need to define this variable manually
or pass it directly as parameter to CMake.

If problems arise during compilation related to these external dependencies, please adjust the
provided paths accordingly for your build system.

### Docker build environment (Linux only)
To simplify the setup of a reproducible build environment, a `Dockerfile` based on an Ubuntu 18.04
base image is provided, which when build includes all necessary dependencies, except the Vimba
version against which vimbasrc should be linked. This is added when the compile command is run by
mounting a Vimba installation into the Docker container.

#### Building the docker image
In order to build the docker image from the `Dockerfile`, run the following command inside the
directory containing it:
```
docker build -t gst-vimbasrc:18.04 .
```

#### Compiling vimbasrc using the Docker image
After running the build command described above, a Docker image with the tag `gst-vimbasrc:18.04`
will be created. This can be used to run the build process of the plugin.

Building the plugin with this image is simply a matter of mounting the source code directory and the
desired Vimba installation directory into the image at appropriate paths, and letting it run the
provided `build.sh` script. The expected paths into which to mount these directories are:
- **/gst-vimbasrc**: Path inside the Docker container in which the gst-vimbasrc project should be
  mounted
- **/vimba**: Path inside the Docker container under which the desired Vimba installation should be
  mounted

The full build command to be executed on the host would be as follows:
```
docker run --rm -it --volume /path/to/gst-vimbasrc:/gst-vimbasrc --volume /path/to/Vimba_X_Y:/vimba gst-vimbasrc:18.04
```

## Installation
GStreamer plugins become available for use in pipelines, when GStreamer is able to load the shared
library containing the desired element. GStreamer typically searches the directories defined in
`GST_PLUGIN_SYSTEM_PATH`. If this variable is not defined, the default paths of the system wide
GStreamer installation, as well as the `~/.local/share/gstreamer-<GST_API_VERSION>/plugins`
directory of the current user are searched. Installing the `vimbasrc` element is therefore simply a
matter of placing the compiled shared library into this search path and letting GStreamer load it.

### Installation dependencies
As the shared library containing the `vimbasrc` element  is dynamically linked, its linked
dependencies need to loadable. As GStreamer itself will likely be installed system wide, the
dependencies on glib and GStreamer libraries should already be satisfied.

In order to satisfy the dependency on `libVimbaC.so` the shared library needs to be placed in an
appropriate entry of the `LD_LIBRARY_PATH`. The exact method for this is a matter of preference and
distribution dependant. On Ubuntu systems one option would be to copy `libVimbaC.so` into
`/usr/local/lib` or to add the directory containing `libVimbaC.so` to the `LD_LIBRARY_PATH` by
adding an appropriate `.conf` file to `/etc/ld.so.conf.d/`.

Correct installation of `libVimbaC.so` can be checked, by searching for its file name in the output
of `ldconfig -v` (e.g.: `ldconfig -v | grep libVimbaC.so`). Alternatively correct loading of
dependent shared libraries can be checked with `ldd` (e.g. `ldd libgstvimbasrc.so`).

## Usage
**The vimbasrc plugin is still in active development. Please keep the _Known issues and limitations_
in mind when specifying your GStreamer pipelines and using it**

`vimbasrc` is intended for use in GStreamer pipelines. The element can be used to forward recorded
frames from a Vimba compatible camera into subsequent GStreamer elements.

The following pipeline can for example be used to display the recorded camera image. The
`camera=<CAMERA-ID>` parameter needs to be adjusted to use the correct camera ID.
```
gst-launch-1.0 vimbasrc camera=DEV_1AB22D01BBB8 ! videoscale ! videoconvert ! queue ! autovideosink
```

### Supported camera features
A list of supported camera features can be found by using the `gst-inspect` tool on the `vimbasrc`
element. This will display a list of available "Element Properties", which include the available
camera features.

For some of the exposed features camera specific restrictions in the allowed values may apply. For
example the `Width`, `Height`, `OffsetX` and `OffsetY` features may only accept integer values
between a minimum and a maximum value in a certain interval. In cases where the provided value could
not be applied a logging message with level `WARNING` will be printed (make sure that an appropriate
logging level is set: e.g. `GST_DEBUG=vimbasrc:WARNING` or higher) and image acquisition will
proceed with the feature values that were initially set on the camera.

### Supported pixel formats
Selecting the desired format can be achieved by defining it in a GStreamer
[capsfilter](https://gstreamer.freedesktop.org/documentation/coreelements/capsfilter.html) element
(e.g. `video/x-raw,format=GRAY8`). A full example pipeline setting the GStreamer GRAY8 format is
shown below. Selecting the GStreamer GRAY8 format will set the Mono8 Vimba Format in the used camera
to record images.
```
gst-launch-1.0 vimbasrc camera=DEV_1AB22D01BBB8 ! video/x-raw,format=GRAY8 ! videoscale ! videoconvert ! queue ! autovideosink
```

Not all Vimba pixel formats can be mapped to compatible GStreamer video formats. This is especially
true for the "packed" formats. The following tables provide a mapping where possible.

#### GStreamer video/x-raw Formats
| Vimba Format        | GStreamer video/x-raw Format |
|---------------------|------------------------------|
| Mono8               | GRAY8                        |
| Mono10              | GRAY16_LE                    |
| Mono12              | GRAY16_LE                    |
| Mono14              | GRAY16_LE                    |
| Mono16              | GRAY16_LE                    |
| RGB8                | RGB                          |
| BGR8                | BGR                          |
| Argb8               | ARGB                         |
| Rgba8               | RGBA                         |
| Bgra8               | BGRA                         |
| Yuv411              | IYU1                         |
| YCbCr411_8_CbYYCrYY | IYU1                         |
| Yuv422              | UYVY                         |
| YCbCr422_8_CbYCrY   | UYVY                         |
| Yuv444              | IYU2                         |
| YCbCr8_CbYCr        | IYU2                         |

#### GStreamer video/x-bayer Formats
The GStreamer `x-bayer` formats in the following table are compatible with the GStreamer
[`bayer2rgb`](https://gstreamer.freedesktop.org/documentation/bayer/bayer2rgb.html) element, which
is able to debayer the data into a widely accepted RGBA format.

| Vimba Format        | GStreamer video/x-bayer Format |
|---------------------|--------------------------------|
| BayerGR8            | grbg                           |
| BayerRG8            | rggb                           |
| BayerGB8            | gbrg                           |
| BayerBG8            | bggr                           |

## Known issues and limitations
- Frame status is not considered in `gst_vimbasrc_create`. This means that incomplete frames may get
  pushed out to the GStreamer pipeline where parts of the frame may contain garbage or old image
  data
    - A warning message is output if an incomplete frame is encountered (only displayed if logging
      level is set appropriately)
