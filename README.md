# tdis
TDIS is the TDIS Data Investigation System

### Docker images

There are two images:

- [**eicdev/tdis-pre**](https://hub.docker.com/r/eicdev/tdis-pre) - with all dependencies but without `tdis` reco built.
  This image is used for CI or development reasons and is not designed for running `tdis`
- [**eicdev/tdis**](https://hub.docker.com/r/eicdev/tdis) - The image can be used to run `tdis`

## Running in docker
*(extended information about options of running this docker containers)*

To download/update the container

```bash
docker pull eicdev/tdis:latest
```

Running docker:

```bash
docker run -it --rm eicdev/tdis:latest bash
```

- ```--rm``` flag Docker **automatically cleans up the container** and 
  remove the file system **when the container exits**.
- ```-it``` flag enables interactive session. Without this flag `ctrl+c` may not work. 
  In general `-it` is used to run e.g. bash session (see below)
- 

**Extremely shallow docker crash course** 

Docker utilizes the pull command to download a package called an **image**. 
When Docker executes the image, it runs within a **container**. 
Each time the `docker run` command is called, a new container is generated.
Docker creates a modifiable layer over the **image** and starts the **container** with the given command.

If the `--rm` flag is not used, stopping the container does not remove it.
A stopped container can be restarted with all its previous changes intact using the docker start command.

Docker functions similarly to tmux or screen, allowing you to reconnect to the running image, 
attach multiple bash shells, and even reconnect if the container is stopped. 
This feature facilitates easier debugging and data retention. 
However, be cautious when using the --rm flag, as it removes the container upon stopping.


Docker documentation:
[docker run](https://docs.docker.com/engine/reference/commandline/run/),
[docker start](https://docs.docker.com/engine/reference/commandline/start/),
[--rm flag](https://docs.docker.com/engine/reference/run/#clean-up---rm).


### Add your system directory inside docker

You can bind any directory on your system to docker image by using **-v** flag:

```bash 
-v <your/directory>:<docker/directory>

# /mnt is a good place to mount directories inside a container
docker run -it --rm -v /host/dir:/mnt/data eicdev/tdis:latest
```
More information on [docker bind](https://docs.docker.com/storage/bind-mounts/),
There are other mechanisms of how to manage data in docker. 
[the official documentation on managing data in docker](https://docs.docker.com/storage/)


**Debugging** : To do C++ debugging (run GDB or so) one has to specify additional flags
(the debugging is switched off by docker by default for security reasons):

```--cap-add=SYS_PTRACE --security-opt seccomp=unconfined```