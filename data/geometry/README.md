# Geometry Lab sample data

sample_cube.stl is a self-created ASCII STL fixture containing four triangles. It is not imported from CAD software.

sample_temperature.csv is a self-created synthetic temperature dataset using metres and degrees Celsius. It is not an ANSYS export and is not registered to the STL coordinate system by the application.

The API accepts UTF-8 text uploads through JSON with filename and content fields and limits request bodies to 2 MiB. Binary STL, quoted/multiline CSV fields, and mesh-to-result registration are outside the current supported format.
