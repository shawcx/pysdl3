SDL3
====

.. image:: https://github.com/shawcx/pysdl3/actions/workflows/tests.yml/badge.svg
   :target: https://github.com/shawcx/pysdl3/actions/workflows/tests.yml
   :alt: tests

Python 3 bindings for `SDL3 <https://libsdl.org>`_ and SDL3_image: windows,
rendering, surfaces, textures, events, input, audio, gamepads, timers and more.
Names drop the ``SDL_`` prefix (``SDL_RenderPresent`` → ``Renderer.Present``,
``SDL_EVENT_QUIT`` → ``SDL3.EVENT_QUIT``).

.. image:: https://raw.githubusercontent.com/shawcx/pysdl3/main/docs/screenshot.png
   :alt: example/draw.py

Install
-------

Needs Python 3.10+ and SDL 3.2+. Install SDL3, then build into a virtualenv::

    brew install sdl3 sdl3_image        # macOS
    sudo apt install build-essential pkg-config python3-dev python3-venv \
        libsdl3-dev libsdl3-image-dev   # Debian 13+ / Ubuntu 25.10+

    python3 -m venv .venv && . .venv/bin/activate
    pip install .

Or build a Debian package, ``python3-sdl3`` (the ``.deb`` lands in ``../``)::

    sudo apt build-dep .
    make deb

Example
-------

.. code-block:: python

    import SDL3

    SDL3.Init(SDL3.INIT_VIDEO)
    window = SDL3.Window('hello', (640, 480))
    renderer = window.CreateRenderer()
    renderer.SetVSync(1)

    running = True
    while running:
        while (event := SDL3.PollEvent()) is not None:
            if event[0] == SDL3.EVENT_QUIT:
                running = False
        renderer.SetRenderDrawColor(16, 16, 24, 255)
        renderer.Clear()
        renderer.SetRenderDrawColor(240, 200, 40, 255)
        renderer.FillRect((270, 190, 100, 100))
        renderer.Present()

    SDL3.Quit()

More in ``example/``; the screenshot is ``python3 example/draw.py docs/screenshot.png``.

Tests
-----

Headless (dummy video / audio drivers); builds the extension if needed::

    pip install pytest setuptools
    python -m pytest

License
-------

MIT
