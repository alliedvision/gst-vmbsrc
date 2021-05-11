# gst-vimbasrc
This project contains the official GStreamer plugin to make cameras supported by Allied Visions
Vimba API available as GStreamer sources.

GStreamer is multimedia framework which assembles pipelines from multiple elements. Using the
`vimbasrc` element it is possible to record images with industrial cameras supported by Vimba and
pass them directly into these pipelines. This enables a wide variety of uses such as live displays
of the image data or encoding them to a video format.

`vimbasrc` is currently officially supported on the following systems and architectures:
- AMD64 (Validated on Ubuntu 18.04)
- ARM64 (Validated on NVIDIA L4T 32.4.4)

The following library versions have been validated to work with `vimbasrc`:
- Vimba 4.2
- GStreamer 1.14

## Building
A `CMakeLists.txt` file is provided that helps build the plugin. For convenience this repository
also contains two scripts (`build.sh` and `build.bat`) that run the appropriate cmake commands for
Linux and Windows systems respectively. They create a directory named `build` in which the project
files, as well as the built binary, are placed.

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
version against which `vimbasrc` is linked. This is added when the compile command is run by mounting
a Vimba installation into the Docker container.

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
- **/gst-vimbasrc**: Path inside the Docker container to mount the gst-vimbasrc project
- **/vimba**: Path inside the Docker container to mount the desired Vimba installation

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

### Setting camera features
To adjust the image acquisition process of the camera, access to settings like the exposure time are
necessary. The `vimbasrc` element provides access to these camera features in one of two ways.
1. If given, an XML file defining camera features and their corresponding values is parsed and all
   features contained are applied to the camera (see [Using an XML file](####Using-an-XML-file))
2. Otherwise selected camera features can be set via properties of the `vimbasrc` element (see
   [Supported via GStreamer properties](####Supported-via-GStreamer-properties))

The first approach allows the user to freely modify all features the used camera supports. The
second one only gives access to a small selection of camera features that are supported by many, but
not all camera models. The feature names (and in case of enum features their values) follow the
Standard Feature Naming Convention (SFNC) for GenICam devices. For cameras not implementing the
SFNC, this may lead to errors in setting some camera features. For these devices the feature setting
via a provided XML file is recommended.

#### Using an XML file
Providing an XML file containing the desired feature values allows access to all supported camera
features. A convenient way to creating such an XML file is configuring your camera as desired with
the help of the VimbaViewer and saving the current configuration as an XML file from there. The path
to this file may then be passed to `vimbasrc` via the `settingsfile` property as shown below
```
gst-launch-1.0 vimbasrc camera=DEV_1AB22D01BBB8 settingsfile=path_to_settings.xml ! videoscale ! videoconvert ! queue ! autovideosink
```

**If a settings file is used no other parameters passed as element properties are applied as feature
values.** This is done to prevent accidental overwriting of previously set features. One exception
from this rule is the format of the recorded image data. For details on this particular feature see
[Supported pixel formats](###Supported-pixel-formats).

#### Supported via GStreamer properties
A list of supported camera features can be found by using the `gst-inspect` tool on the `vimbasrc`
element. This displays a list of available "Element Properties", which include the available camera
features. **Note that these properties are only applied to their corresponding feature, if no XML
settings file is passed!**

For some of the exposed features camera specific restrictions in the allowed values may apply. For
example the `Width`, `Height`, `OffsetX` and `OffsetY` features may only accept integer values
between a minimum and a maximum value in a certain interval. In cases where the provided value could
not be applied a logging message with level `WARNING` is printed (make sure that an appropriate
logging level is set: e.g. `GST_DEBUG=vimbasrc:WARNING` or higher) and image acquisition will
proceed with the feature values that were initially set on the camera.

In addition to the camera features listed by `gst-inspect`, the pixel format the camera uses to
record images can be influenced. For details on this see [Supported pixel
formats](###Supported-pixel-formats).

### Supported pixel formats
As the pixel format has direct impact on the layout of the image data that is moving down the
GStreamer pipeline, it is necessary to ensure, that linked elements are able to correctly interpret
the received data. This is done by negotiating the exchange format of two elements by finding a
common data layout they both support. Supported formats are reported as `caps` of an elements pad.
In order to support this standard negotiation procedure, the pixel format is therefore set depending
on the negotiated data exchange format, instead of as a general element property like the other
camera features.

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
| Vimba Format        | GStreamer video/x-raw Format | Comment                                                                     |
|---------------------|------------------------------|-----------------------------------------------------------------------------|
| Mono8               | GRAY8                        |                                                                             |
| Mono10              | GRAY16_LE                    | Only the 10 least significant bits are filled. Image will appear very dark! |
| Mono12              | GRAY16_LE                    | Only the 12 least significant bits are filled. Image will appear very dark! |
| Mono14              | GRAY16_LE                    | Only the 14 least significant bits are filled. Image will appear very dark! |
| Mono16              | GRAY16_LE                    |                                                                             |
| RGB8                | RGB                          |                                                                             |
| RGB8Packed          | RGB                          | Legacy GigE Vision Format. Does not follow PFNC                             |
| BGR8                | BGR                          |                                                                             |
| BGR8Packed          | BGR                          | Legacy GigE Vision Format. Does not follow PFNC                             |
| Argb8               | ARGB                         |                                                                             |
| Rgba8               | RGBA                         |                                                                             |
| Bgra8               | BGRA                         |                                                                             |
| Yuv411              | IYU1                         |                                                                             |
| Yuv411Packed        | IYU1                         | Legacy GigE Vision Format. Does not follow PFNC                             |
| YCbCr411_8_CbYYCrYY | IYU1                         |                                                                             |
| Yuv422              | UYVY                         |                                                                             |
| Yuv422Packed        | UYVY                         | Legacy GigE Vision Format. Does not follow PFNC                             |
| YCbCr422_8_CbYCrY   | UYVY                         |                                                                             |
| Yuv444              | IYU2                         |                                                                             |
| Yuv444Packed        | IYU2                         | Legacy GigE Vision Format. Does not follow PFNC                             |
| YCbCr8_CbYCr        | IYU2                         |                                                                             |


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

## Troubleshooting
- How can I enable logging for the plugin
  - To enable logging set the `GST_DEBUG` environment variable to `GST_DEBUG=vimbasrc:DEBUG` or
    another appropriate level. For further details see [the official
    documentation](https://gstreamer.freedesktop.org/documentation/tutorials/basic/debugging-tools.html)
- The camera features are not applied correctly
  - Not all cameras support access to their features via the names agreed upon in the Standard
    Feature Naming Convention. If your camera uses different names for its features, consider [using
    an XML file to pass camera settings](####Using-an-XML-file) instead.
- When displaying my images I only see black
  - This may be due to the selected pixel format. If for example a Mono10 pixel format is chosen for
    the camera, the resulting pixel intensities are written to 16bit fields in the used image buffer
    (see table in [video/x-raw formats overview](####GStreamer-video/x-raw-Formats)). Because the
    pixel data is only written to the least significant bits, the used pixel intensity range does
    not cover the expected 16bit range. If the display assumes the 16bit data range to be fully
    utilized, your recorded pixel intensities may be too small to show up on your display, because
    they are simply displayed as very dark pixels.
- When displaying my images I only see green
  - This is possibly caused by an error in the `videoconvert` element. Try enabling error messages
    for all elements (`GST_DEBUG=ERROR`) to see if there is a problem with the size of the data
    buffer. If that is the case check the troubleshooting entry to "The `videoconvert` element
    complains about too small buffer size"
- The `videoconvert` element complains about too small buffer size
  - This is most likely caused by the width of the image data not being evenly divisible by 4 as the
    `videoconvert` element expects. Try setting the width to a value that is evenly divisible by 4.

## Known issues and limitations
- In situations where cameras submit many frames per second, visualization may slow down the
  pipeline and lead to a large number of incomplete frames. For incomplete frames warnings are
  logged. The user may select whether they want to drop incomplete frames (default behavior) or to
  submit them into the pipeline for processing. Incomplete frames may contain pixel intensities from
  old acquisitions or random data. The behavior is selectable with the `incompleteframehandling`
  property.
- Complex camera feature setups may not be possible using the provided properties (e.g. complex
  trigger setups for multiple trigger selectors). For those cases it is recommended to [use an XML
  file to pass the camera settings](####Using-an-XML-file).
- If the width of the image data pushed out of the `gst-vimbasrc` element is not evenly divisible by
  4, the image data can not be transformed with the `videoconvert` element. This is caused by the
  `videoconvert` element expecting the width to always be a multiple of 4, or being explicitly told
  the stride of the image data in the buffer. This explicit stride information is currently not
  implemented. For now it is therefore recommended to use `width` settings that are evenly divisible
  by 4.
