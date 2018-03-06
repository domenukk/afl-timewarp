#!/bin/bash
#Vagrant bootstrapping, inspired by https://github.com/Brown-University-Library/django-vagrant/blob/master/provision/bootstrap.sh
#This is what needs to be done to get this vagrant thing rolling.
#It'll install a python virtualenv etc.

HOME=/home/ubuntu/
REQS=/vagrant/requirements.txt
VENV=overmind

export DEBIAN_FRONTEND=noninteractive
set -e # Exit script immediately on first error.
set -x # Print commands and their arguments as they are executed.

#Install mongo
sudo apt-key adv --keyserver hkp://keyserver.ubuntu.com:80 --recv EA312927
sudo bash -c 'echo "deb http://repo.mongodb.org/apt/ubuntu xenial/mongodb-org/3.2 multiverse" > /etc/apt/sources.list.d/mongodb-org-3.2.list'

sudo apt-get update -y

sudo apt-get install -y mongodb-org virtualenvwrapper git vim 

sudo service mongod start

echo "Mongo Started :)"

#basics
sudo apt-get install -y git-core vim tmux wget curl zsh
wget --no-check-certificate https://github.com/robbyrussell/oh-my-zsh/raw/master/tools/install.sh -O - | sh
#sed -i 's/ZSH_THEME="gentoo"/ZSH_THEME="3den"/g' $HOME.zshrc
sudo chsh -s /bin/zsh ubuntu

sudo apt-get install -y gcc bison make libtool gdb autoconf libc-dev openssl libpcap-dev

#System Python (2.7) (bdj- neede?)
sudo apt-get install -y python3 python3-dev python3-virtualenv

wget https://bitbucket.org/pypy/pypy/downloads/pypy3-v5.9.0-linux64.tar.bz2 -O /opt/pypy3-v5.9.0-linux64.tar.bz2
cd /opt/
tar jxvf pypy3-v5.9.0-linux64.tar.bz2 

#Venv Setup
#if [[ ! -e $HOME$VENV ]]; then
#mkdir $HOME$VENV
virtualenv -p /opt/pypy3-v5.9.0-linux64/bin/pypy3 $HOME$VENV
#fi


ACTIVATE=$HOME$VENV/bin/activate
source $ACTIVATE
pip3 install 'ipython==1.2.1'

# Insatll all deps
pip3 install -r $REQS


echo $ACTIVATE >> ~/.bashrc
echo $ACTIVATE >> ~/.zshrc

echo "Done :)"



