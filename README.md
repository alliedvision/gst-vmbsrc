# gst-vimba
This project contains a plugin to make cameras supported by Allied Vision Technologies Vimba API
available as GStreamer sources.

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
docker build -t gst-vimba:18.04 .
```

#### Compiling vimbasrc using the Docker image
After running the build command described above, a Docker image with the tag `gst-vimba:18.04` will
be created. This which can be used to run the build process of the plugin.

Building the plugin with this image is simply a matter of mounting the source code directory and the
desired Vimba installation directory into the image at appropriate paths, and letting it run the
provided `build.sh` script. The expected paths into which to mount these directories are:
- **/gst-vimba**: Path inside the Docker container in which the gst-vimba project should be mounted
- **/vimba**: Path inside the Docker container under which the desired Vimba installation should be
  mounted

The full build command to be executed on the host would be as follows:
```
docker run --rm -it --volume /path/to/gst-vimba:/gst-vimba --volume /path/to/Vimba_X_Y:/vimba gst-vimba:18.04
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
dependent shared libraries can be checked with `ldd` (e.g. `ldd libgstvimba.so`).

## Usage
**The vimbasrc plugin is still in active development. Please keep the _Known issues and limitations_
in mind when specifying your GStreamer pipelines and using it**

`vimbasrc` is intended for use in GStreamer pipelines. The element can be used to forward recorded
frames from a Vimba compatible camera into subsequent GStreamer elements.

The following pipeline can for example be used to display the recorded camera image. The
`camera=<CAMERA-ID>` parameter needs to be adjusted to use the correct camera ID. The `width` and
`height` parameters are used to control the output size of the recorded camera images. These
features correspond to the `Width` and `Height` camera features and crop the recorded sensor area if
they are not set to the full sensor size.
```
gst-launch-1.0 vimbasrc camera=DEV_1AB22D01BBB8 ! video/x-raw,width=2592,height=1944 ! videoscale ! videoconvert ! queue ! autovideosink
```

### Supported camera features
A list of supported camera features can be found by using the `gst-inspect` tool on the `vimbasrc`
element. This will display a list of available "Element Properties", which include the available
camera features.

## Known issues and limitations
- Frame status is not considered in `gst_vimbasrc_create`. This means that incomplete frames may get
  pushed out to the GStreamer pipeline where parts of the frame may contain garbage or old image
  data
- `width` and `height` are not considered correctly. Currently resolution values for Alvium 500m
  cameras are hard-coded. Other sensor sizes might or might not work but display will definitely be
  incorrect
- `gst_vimbasrc_set_caps` currently does not set negotiated settings on the camera.
- Only the `Mono8` Vimba pixel format (GStreamer equivalent: `video/x-raw,format=GRAY8`) is
  supported and hard coded into `gst_vimbasrc_get_caps`. The pixel format is not adjusted on the
  camera but instead the default format is kept. On color cameras this might lead to incorrect
  image/errors data due to pixel layout differences and unexpected buffer sizes
- When closing the camera connection the buffer storing the `camera_id` contains unexpected
  characters. Buffer might be emptied too early which could have an impact on the `VmbCameraClose`
  call
