## Set Up the Test Server
Obtain a copy of `pure-ftpd`, and configure it to run as a non-privileged process:
```shell
./configure --prefix=<installation path> --with-nonroot
```
Then install it:
```shell
make install-strip
```
Finally, make sure this `pure-ftpd` is accessible in your search path.
