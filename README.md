# Systemback

This is a fork of systemback. The [original project](https://launchpad.net/systemback) is no longer maintained by the creator.

Simple system backup and restore application with extra features

Systemback makes it easy to create backups of the system and the users configuration files. In case of problems you can easily restore the previous state of the system. There are extra features like system copying, system installation and Live system creation.

## Install

```bash
sudo sh -c 'echo "deb [arch=amd64] http://mirrors.bwbot.org/ stable main" > /etc/apt/sources.list.d/systemback.list'
sudo apt-key adv --keyserver 'hkp://keyserver.ubuntu.com:80' --recv-key 50B2C005A67B264F
sudo apt-get update
sudo apt-get install systemback
```

## Build

```bash
git clone https://github.com/BluewhaleRobot/systemback
cd systemback/systemback
debuild
```

## Changelog

1.8.9

- Fix support for NVMe

1.8.8

- Merge from https://github.com/fconidi/Systemback_source-1.9.4
- Add support for NVMe

1.8.7

- Fix symbol link missing after install system images

1.8.6

- Add sbignore file, set include user data as default, set autoiso as default

1.8.5

- Add support for Ubuntu 20.04 and large iso
