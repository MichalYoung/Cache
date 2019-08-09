# Building and running the Cache in Docker

A build in Docker requires access to all the libraries that 
the Cache uses, which means we must have access to SRPC and ADTs. 
This makes it impossible to build the Cache with a Dockerfile within 
the Cache directory, unless one of the following is true: 

* SRPC and ADTs are subdirectories of the Cache.   (This might work 
  with GIT submodule; I have not tried that.  I also have not tried 
  symbolic linking, which would involve a manual build step that I 
  want to avoid.) 
  
* SRPC and ADTs are already in the base Docker image on which 
  the Cache image is built.  This is probably the better long-term 
  approach, as discussed below. 
  
* SRPC, ADTs, and the Cache are subdirectories of an umbrella directory, 
  where the Dockerfile lives.  The Dockerfile in the umbrella directory 
  can access all three.   
  
The approach that does *not* work is building Cache from a base Linux 
image directly from Cache, without one of the conditions above.  The 
reason is that the Dockerfile would need an instruction like 

```
COPY ../SRPC /usr/src/SRPC
```

or 

```
COPY /my/work/directory/SRPC /usr/src/SRPC
```

and in either case Docker will refuse to access the SRPC sources 
through a relative or absolute path outside the "scope" of the Dockerfile. 
Anything that is COPYd into the image must be within the same directory 
as the Dockerfile. 

## Building from an Umbrella Directory 

It is possible to construct the Cache repository from a base Linux 
image with one Dockerfile. 
Place Cache, ADTs, and SRPC repositories 
in a parent directory (the umbrella) and  place the Dockerfile in the 
umbrella directory.  Then multiple COPY statements can install all 
three in the image and build them.  

```
COPY ADTs /usr/src/ADTs
WORKDIR /usr/src/ADTs
RUN autoreconf -if &&\
    ./configure &&\
    make clean &&\
    make &&\
    make install &&\
    make check

COPY SRPC /usr/src/SRPC
WORKDIR /usr/src/SRPC
RUN autoreconf -if &&\
    ./configure &&\
    make clean &&\
    make &&\
    make install

COPY Cache2019 /usr/src/Cache
WORKDIR /usr/src/Cache
RUN   libtoolize &&\
      autoreconf -if &&\
      ./configure &&\
      make clean &&\
      make  &&\
      make install
```

I will create a git repository with instructions for producing 
a Docker image in this fashion.  It is a relatively simple 
approach and works, but it is somewhat at odds with standard 
practices with Docker.  A central idea of Docker, supported 
by Docker storage of images as change sets and an underlying 
copy-on-write mechanism for the file system, is maintaining 
a layered system as a set of distinct images.  

### What's wrong wth the umbrella directory approach? 

A drawback of the umbrella directory approach is that it requires 
manually assembling the directories in a fixed structure, and placing 
the dockerfile in a directory that is not a GIT repository (although a 
dummy GIT repository could be created with only the Dockerfile and 
instructions for constructing the rest).  It's not a disaster, but 
it's probably not the best way to build the Cache. 

## Layering images 

A central theme of Docker is layering images: Each Docker image is 
build FROM a base image (with the empty base image as a special base 
case).  Currently I am building the Cache directly on an Ubuntu Linux
image, and building ADTs, SRPC, and then the Cache in that image. What I
probably should be doing is building three images: 

* An ADTs image built on top of the base (Ubuntu or another Linux)
* An SRPC image built on top of the ADTs image 
* The Cache image built on top of the SRPC image

This is in the spirt of Docker, and should be handled efficiently 
by it.  

In my present working version of the cache project, I have a Dockerfile 
at the top level of each of ADTs, SRPC, and Cache.  