#!/bin/bash
docker run -p 80:80/tcp --rm --name petdisk -v $(pwd)/www:/var/www/html php:7.4-apache

