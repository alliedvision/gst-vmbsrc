# gst-vimba
This project contains a plugin to make cameras supported by Allied Vision Technologies Vimba API
available as GStreamer sources.

## Building
A CMakeLists.txt file is provided that should be used to build the plugin. For convenience this
repository also contains two scripts (`build.sh` and `build.bat`) that run the appropriate cmake
commands for Linux and Windows systems respectively. They will create a directory named `build` in
which the project files, as well as the built binary, will be placed.

### Docker build environment (Linux only)
To simplify the setup of a reproducible build environment, a `Dockerfile` based on an Ubuntu 18.04
base image is provided, which when build includes all necessary dependencies. In order to build the
docker image from this file, run the following command:
```
docker build -t gst-vimba:18.04 .
```

This will create a docker image with the tag `gst-vimba:18.04`, which can be used to run the build
process of the plugin. Building the plugin with this image is simply a matter of mounting the source
code directory into the image, and letting it run the provided `build.sh` script. This can be done
with the following command:
```
docker run --rm -it --volume /path/to/gst-vimba:/gst-vimba gst-vimba:18.04
```
