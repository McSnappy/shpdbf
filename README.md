# shpdbf
Partial implementation of the Esri shapefile format
-----------------------------

This project is a slap-dash, partial implementation of the standard meant for learning purposes only. Look elsewhere for quality code.

Currently the code can read and write shapefiles/dbfs that contain point, multipoint, line, or polygon feature classes (doesn't handle Z or M data).

The included sample program (shptest) creates a simple point feature class.

useful links:
[Esri Shapefile Specification](https://www.esri.com/content/dam/esrisites/sitecore-archive/Files/Pdfs/library/whitepapers/pdfs/shapefile.pdf "shapefile spec") 

[dBASE III File Format](https://web.archive.org/web/20190311062710/http://www.oocities.org/geoff_wass/dBASE/GaryWhite/dBASE/FAQ/qformt.htm "dBASE III File Format") 
 
 
Build and run shptest:
---------------------
    cd src  
    make  
    ./shptest  



Copyright (c) Carl Sherrell

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

