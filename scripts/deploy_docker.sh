#!/usr/bin/env bash
docker build -t substr_search . &&
    docker run -it --rm --name substr_search_bench substr_search
