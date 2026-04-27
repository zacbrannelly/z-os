FROM openeuler/edk2:latest

# Install dependencies
RUN yum install -y dosfstools mtools xorriso cmake

CMD ["bash"]
