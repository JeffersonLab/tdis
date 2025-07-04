# IFarm tutorial

**In this tutorial we will use docker/podman containers to build tdis software on ifarm.**

IFarm stands for "Interactive Farm" a part of Jefferson Lab scientific computing environment.
Users are free to use ifarm nodes for software development and testing, but do not run long tasks.

More information:
- [containers and tdis images](containers.md)
- [how to build tdis software](install.md)
- [login to ifarm ↗](https://jlab.servicenowservices.com/kb?id=kb_article&sysparm_article=KB0014687)
- [using containers at JLab ↗](https://pages.jlab.org/scicomp/software/jlab-container-docs/)

## Using podman

Podman or `podman` command should be available on ifarm our of the box after login.
`podman` have the same subcommands as `docker`, on ifarm `docker` is an alias to podman

```bash
# run podman command:
podman --version
```

To get the image needed to build `tdis` software: 

```bash
podman pull docker.io/eicdev/tdis-pre:latest
```

> The same command could be used to update the image in future

After the image is downloaded, you should be able to get into the bash shell with command: 

```bash
podman run -it --rm eicdev/tdis-pre bash
```

Finally, you need to bind directories from CUE to container.
You can bind any directory on your system to docker image by using **-v** flag:

```
-v <your/directory>:<docker/directory>
```

Binding `/work/halla/tdis` to docker as `/tdis` 

```bash
podman run --rm  -it -v /work/halla/tdis:/tdis eicdev/tdis-pre:latest bash
ls /tdis
```

After `ls /tdis` you should see in container the content of CUE `/work/halla/tdis`

Now you can follow [install](install.md) page with software installation instructions.


## Build software

**(!!!) use your username `/work/halla/tdis/<username>` directory for your work (!!!)**

Lets build `tdis` software in container

```bash
podman run --rm  -it -v /work/halla/tdis/<username>:/tdis eicdev/tdis-pre:latest bash

# Go to directory shared between the host and the container:
cd /tdis

# Clone TDIS software
git clone https://github.com/JeffersonLab/tdis
cd tdis

# Build cmake project
mkdir build && cd build
cmake ..
make -j8 && make install
```


## Troubleshooting

One of the possible problems with podman on CUE is that if it is configured incorrectly, it saves 
images and other data to users home directory. CUE user directories have comparably small quotas as 
they are being backup-ed. To check your configuration:   

```bash
podman system info
```

It should provide lots of information and information about where data is saved is scattered there
(you should see your user name instead of **username** ): 


```
store:
  ...
  configFile:  /u/home/<username>/.config/containers/storage.conf
  graphRoot:  /scratch/<username>/.local/share/containers/storage
  volumePath: /scratch/<username>/.local/share/containers/storage/volumes
  imageCopyTmpDir: /scratch/<username>
```

If it is not set correctly, options are: 

- Contact CIT or create an incident. They will help.  
- Search how it is configured for podman

