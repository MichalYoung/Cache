#! /bin/sh
#
#  Run a docker container with the cache,
#  mapping /var/src/cache from host system
#

# Note Docker cannot mount a host directory with a space in its path,
# e.g., anything in "Google Drive".  We get:
# docker: Error response from daemon: create /Users/michal/Google Drive/systems/cache_project/Cache2019:
# "/Users/michal/Google Drive/systems/cache_project/Cache2019" includes invalid characters for a local
# volume name, only "[a-zA-Z0-9][a-zA-Z0-9_.-]" are allowed. If you intended to pass a host directory,
# use absolute path.
# Note problem is NOT that it's not an absolute path (it is), but that the absolute path has a
# space in it.  Escaping it doesn't help.  A symbolic link is a very hacky workaround.
#
# Symbolic linking does seem to work with explicit type=bind
#

rm -f /tmp/cache2019_src
ln -s "$(pwd)/src" /tmp/cache2019_src

ls /tmp/cache2019_src

docker run -it --mount type=bind,source=/tmp/cache2019_src,target=/var/src/cache2019 michalyoung/cache2019:cache

