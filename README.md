# Tiny Assistant

Tiny Assistant is an LLM-based assistant that runs on your Pebble smartwatch,
if you still have a smartwatch that ceased production in 2016 lying around.

![A screenshot from a Pebble smartwatch running the Tiny Assistant. The user asked for the time, the assistant responded that it was 3:59 PM.](./docs/screenshot.png)

## Usage

### Server

To use Tiny Assistant, you will need to run the server in `service/` somewhere
your phone can reach.

You will also need to set a few environment variables:

- `GEMINI_KEY` - a key for Google's Gemini - you can get one at the
  [Google AI Studio](https://aistudio.google.com)
- `REDIS_URL` - a URL for a functioning Redis server. No data is persisted
  long-term, so a purely in-memory server is fine.

### Client

Update the URL in `app/src/pkjs/urls.js` to point at your instance of the
server.

Then you can simply build it using the Pebble SDK and install on your watch.

## Contributing

See [`CONTRIBUTING.md`](CONTRIBUTING.md) for details.

## License

Apache 2.0; see [`LICENSE`](LICENSE) for details.

## Disclaimer

This project is not an official Google project. It is not supported by
Google and Google specifically disclaims all warranties as to its quality,
merchantability, or fitness for a particular purpose.
  
