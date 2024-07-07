#!/usr/bin/bash

NAME="Odysseus-lightstone"

DATE=$(date +%Y%m%d)
DIR=${NAME}-$DATE

mkdir $DIR
unzip -d $DIR liblightstone-1.5.zip
mkdir $DIR/liblightstone-1.5/build

git -C $DIR/liblightstone-1.5 clone https://github.com/qdot/compily_buildd.git


cp main.cpp make.sh INSTALL README 50-wilddivine-rules *.wav *.ttf $DIR

tar cvfz ${DIR}.tar.gz $DIR
