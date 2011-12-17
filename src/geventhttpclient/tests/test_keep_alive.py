from geventhttpclient.response import HTTPResponse
from geventhttpclient._parser import HTTPParseError
import pytest


def test_keep_alive():
    response = HTTPResponse()
    response.feed("""HTTP/1.1 200 Ok\r\n\r\n""")
    assert response.should_keep_alive()
    assert response.status_code == 200

def test_keep_alive_http_10():
    response = HTTPResponse()
    response.feed("""HTTP/1.0 200 Ok\r\n\r\n""")
    assert not response.should_keep_alive()
    assert response.status_code == 200

def test_keep_alive_bodyless_response_with_body():
    response = HTTPResponse(bodyless=True)
    response.feed("HTTP/1.1 200 Ok\r\n\r\n")
    assert response.should_keep_alive()

    response = HTTPResponse(bodyless=True)
    with pytest.raises(HTTPParseError):
        response.feed(
            """HTTP/1.1 200 Ok\r\nContent-Length: 10\r\n\r\n0123456789""")
    assert not response.should_keep_alive()

def test_keep_alive_bodyless_10x_request_with_body():
    response = HTTPResponse()
    response.feed("""HTTP/1.1 100 Continue\r\n\r\n""")
    assert response.should_keep_alive()

    response = HTTPResponse()
    response.feed("""HTTP/1.1 100 Continue\r\nTransfer-Encoding: identity\r\n\r\n""")
    assert not response.should_keep_alive()
    response = HTTPResponse()
    response.feed("""HTTP/1.1 100 Continue\r\nTransfer-Encoding: chunked\r\n\r\n""")
    assert not response.should_keep_alive()

