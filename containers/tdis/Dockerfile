#
# docker build -t eicdev/eicrecon-ubuntu22-prereq:latest .
# docker build -t eicdev/eicrecon-ubuntu22-prereq:latest --build-arg BUILD_THREADS=24 .
# docker push eicdev/eicrecon-ubuntu22-prereq:latest
#
# docker run --rm -it --init eicdev/eicrecon-ubuntu22-prereq:latest
# docker run --rm -it --init -p8888:8888 eicdev/eicrecon-ubuntu22-prereq:latest

FROM eicdev/tdis-pre:latest

USER eicuser
# aa
RUN python3 -m pip install --user --upgrade --force-reinstall  edpm &&\
    edpm install tdis

    # jupyterab
RUN python3 -m pip install jupyterlab

RUN echo "done building image"
