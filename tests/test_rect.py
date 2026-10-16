'''Phase 9: rect / point geometry helpers.'''

import pytest


def test_has_intersection(sdl):
    assert sdl.HasIntersection((0, 0, 10, 10), (5, 5, 10, 10)) is True
    assert sdl.HasIntersection((0, 0, 5, 5), (10, 10, 5, 5)) is False


def test_intersect_rect(sdl):
    assert sdl.IntersectRect((0, 0, 10, 10), (5, 5, 10, 10)) == (5, 5, 5, 5)
    assert sdl.IntersectRect((0, 0, 5, 5), (10, 10, 5, 5)) is None


def test_union_rect(sdl):
    assert sdl.UnionRect((0, 0, 4, 4), (10, 10, 4, 4)) == (0, 0, 14, 14)


def test_enclose_points(sdl):
    assert sdl.EnclosePoints([(2, 3), (8, 1), (5, 9)]) == (2, 1, 7, 9)
    assert sdl.EnclosePoints([(2, 3), (8, 1)], (100, 100, 10, 10)) is None
    assert sdl.EnclosePoints([(5, 5), (7, 5)], (0, 0, 100, 100)) == (5, 5, 3, 1)


def test_intersect_rect_and_line(sdl):
    assert sdl.IntersectRectAndLine((0, 0, 10, 10), (-5, 5, 20, 5)) == (0, 5, 9, 5)
    assert sdl.IntersectRectAndLine((0, 0, 10, 10), (-5, -5, -1, -1)) is None


def test_point_in_rect(sdl):
    assert sdl.PointInRect((5, 5), (0, 0, 10, 10)) is True
    assert sdl.PointInRect((15, 5), (0, 0, 10, 10)) is False
    assert sdl.PointInRect((10, 5), (0, 0, 10, 10)) is False  # right edge is exclusive


def test_rect_empty_and_equals(sdl):
    assert sdl.RectEmpty((0, 0, 0, 10)) is True
    assert sdl.RectEmpty((0, 0, 1, 1)) is False
    assert sdl.RectEquals((1, 2, 3, 4), (1, 2, 3, 4)) is True
    assert sdl.RectEquals((1, 2, 3, 4), (1, 2, 3, 5)) is False


def test_bad_args_raise(sdl):
    with pytest.raises(TypeError):
        sdl.HasIntersection((0, 0, 1, 1))  # needs two rects
    with pytest.raises((ValueError, TypeError)):
        sdl.RectEmpty('nope')


def test_float_rect_helpers(sdl):
    assert sdl.HasIntersectionF((0.0, 0.0, 10.0, 10.0), (5.0, 5.0, 10.0, 10.0)) is True
    assert sdl.IntersectFRect((0, 0, 10, 10), (5, 5, 10, 10)) == (5.0, 5.0, 5.0, 5.0)
    assert sdl.UnionFRect((0, 0, 4, 4), (10, 10, 4, 4)) == (0.0, 0.0, 14.0, 14.0)
    assert sdl.PointInFRect((5.5, 5.5), (0, 0, 10, 10)) is True
    # SDL3 SDL_RectEmptyFloat treats only strictly-negative extents as empty.
    assert sdl.FRectEmpty((0, 0, -1, 5)) is True
    assert sdl.FRectEquals((1, 2, 3, 4), (1, 2, 3, 4)) is True


# --- phase 13: float enclose / line clip -----------------------------------
# SDL3's float versions have no integer "+1" edges (SDL2's kept them): the
# enclosing rect spans exactly min..max and a rect's far edge x + w is inside.

def test_enclose_fpoints(sdl):
    assert sdl.EncloseFPoints([(0.5, 1.5), (3.25, 2.0)]) == (0.5, 1.5, 2.75, 0.5)
    assert sdl.EncloseFPoints([[1, 1]]) == (1.0, 1.0, 0.0, 0.0)  # lists work too
    assert sdl.EncloseFPoints([(9, 9)], (0, 0, 5, 5)) is None
    assert sdl.EncloseFPoints([(0.25, 0.25), (9, 9)], (0, 0, 5, 5)) == (0.25, 0.25, 0.0, 0.0)
    assert sdl.EncloseFPoints([(1, 1), (5, 5)], (0, 0, 5, 5)) == (1.0, 1.0, 4.0, 4.0)  # far edge inside
    assert sdl.EncloseFPoints([]) is None
    with pytest.raises((TypeError, ValueError)):
        sdl.EncloseFPoints([(1, 2, 3)])
    # The integer version keeps SDL's +1 (pixel) edges.
    assert sdl.EnclosePoints([(1, 1), (5, 5)], (0, 0, 5, 5)) == (1, 1, 1, 1)
    assert sdl.EnclosePoints([]) is None


def test_intersect_frect_and_line(sdl):
    assert sdl.IntersectFRectAndLine((0, 0, 10, 10), (-5, 5, 15, 5)) == (0.0, 5.0, 10.0, 5.0)
    assert sdl.IntersectFRectAndLine((0, 0, 10, 10), (-5, -5, 15, 15)) == (0.0, 0.0, 10.0, 10.0)
    assert sdl.IntersectFRectAndLine((0, 0, 1, 1), [5, 5, 6, 6]) is None  # lists work too
    with pytest.raises(TypeError):
        sdl.IntersectFRectAndLine((0, 0, 1, 1), (1, 2, 3))


def test_intersect_rect_and_line_accepts_a_list(sdl):
    # Regression: a list line went straight to PyArg_ParseTuple (SystemError).
    assert sdl.IntersectRectAndLine((0, 0, 10, 10), [-5, 5, 20, 5]) == (0, 5, 9, 5)
    with pytest.raises(TypeError):
        sdl.IntersectRectAndLine((0, 0, 10, 10), (1, 2, 3))
