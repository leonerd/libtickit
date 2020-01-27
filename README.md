# tickit

Terminal Interface Construction Kit

This library provides an abstracted mechanism for building interactive
full-screen terminal programs. It provides a full set of output drawing
functions, and handles keyboard and mouse input events.

Using this library, applications can:

- Divide the terminal into a hierarchy of nested, possibly-overlapping
  rectangular windows
- Render output content and react to input events independently in any window
  region
- Use fully Unicode-aware string content, including non-BMP, fullwidth and
  combining characters
- Draw line-art using Unicode box-drawing characters in a variety of styles
- Operate synchronously or asynchronously via file descriptors, or abstractly
  via byte buffers
- Recognise arbitrary keyboard input, including modifiers\*
- Make use of multiple terminals, if availble, from a single application

The following terminal features are supported:

- Many rendering attributes; bold, italics\*, underline, reverse,
  strikethough\*, alternate font\*
- 256 and 24-bit (16million) colours\*
- Mouse including mouse wheel and recognition of position reporting greater
  than 224 columns\*
- Arbitrary scrolling regions\*

\*: Not all terminals may support these features

## Project Management

The main project management site, containing bug lists, feature blueprints,
roadmaps, and a mirror of the source code is found on
[Launchpad](https://launchpad.net/libtickit).

Patches and bug reports are also accepted on GitHub.

## Documentation

[Online versions of the manpages](http://www.leonerd.org.uk/code/libtickit/doc/)
are available in HTML form.

## Source

The primary upstream revision control system lives in Bazaar at
[http://bazaar.leonerd.org.uk/c/libtickit](http://bazaar.leonerd.org.uk/c/libtickit).

A fast-import mirror of this is maintained on github at
[https://github.com/leonerd/libtickit](https://github.com/leonerd/libtickit).
Currently this mirror is an experiment at bzr-to-git gatewaying so I don't
guarantee I won't have to force-push this again sometime.

This is also mirrored by the Launchpad project above.
