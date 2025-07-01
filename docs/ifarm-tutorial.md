# IFarm tutorial

IFarm stands for "Interactive Farm" a part of Jefferson Lab scientific computing environment.
Users are free to use ifarm nodes for software development and testing, but do not run long tasks.

https://jlab.servicenowservices.com/kb?id=kb_article&sysparm_article=KB0014687

In this tutorial we will use docker/podman containers. For more information [on containers and TDIS images](containers.md)

## Podman

Podman should be available on ifarm our of the box after login. 

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

Now you can follow [isntall](install.md) page with software installation instructions. 


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

