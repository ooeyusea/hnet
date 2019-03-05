#cmake file for project all
#author:ooeyusea
#created:2015-07-08 09:38:00

cmake_minimum_required(VERSION 2.8)
PROJECT(all)
ADD_SUBDIRECTORY(${PROJECT_SOURCE_DIR}/coroutine)
ADD_SUBDIRECTORY(${PROJECT_SOURCE_DIR}/test)
