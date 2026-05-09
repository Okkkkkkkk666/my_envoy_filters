#!/bin/bash

for file_name in "local_file_xds.yaml" "resources_listener.yaml" "resources_route.yaml" "resources_plugins.yaml"; do
  echo "moving $file_name"
  cp $file_name ${file_name}.bak
  mv ${file_name}.bak $file_name
done