SDL3
====

Python 3 bindings for `SDL3 <https://libsdl.org>`_ and SDL3_image: windows,
rendering, surfaces, textures, events, input, audio, gamepads, timers and more.
Names drop the ``SDL_`` prefix (``SDL_RenderPresent`` → ``Renderer.Present``,
``SDL_EVENT_QUIT`` → ``SDL3.EVENT_QUIT``).

.. image:: https://raw.githubusercontent.com/shawcx/pysdl3/main/docs/screenshot.png
   :alt: example/draw.py

Install
-------

::

    brew install sdl3 sdl3_image                    # macOS
    sudo apt install libsdl3-dev libsdl3-image-dev  # Debian 13+ / Ubuntu 25.10+
    pip install .

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

::

    pip install pytest
    python3 -m pytest

License
-------

MIT
