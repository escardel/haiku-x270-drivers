from http.server import SimpleHTTPRequestHandler, HTTPServer
from urllib.parse import urlsplit, parse_qs
import os

class Handler(SimpleHTTPRequestHandler):
    def do_GET(self):
        if urlsplit(self.path).path == '/log':
            message = parse_qs(urlsplit(self.path).query).get('m', [''])[0]
            print(message, flush=True)
            self.send_response(204)
            self.end_headers()
        else:
            super().do_GET()

    def log_message(self, format, *args):
        pass

os.chdir('/boot/home/x270-camera-mic')
HTTPServer(('127.0.0.1', 8765), Handler).serve_forever()
