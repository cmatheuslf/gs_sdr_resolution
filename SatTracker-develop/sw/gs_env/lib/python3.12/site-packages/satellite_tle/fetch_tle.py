import csv
import requests
import logging

from importlib.resources import files as get_importlib_files
from io import BytesIO, StringIO
from urllib.parse import unquote, urlparse
from zipfile import ZipFile, is_zipfile

from sgp4.earth_gravity import wgs72
from sgp4.io import twoline2rv
from spacetrack import SpaceTrackClient

logger = logging.getLogger(__name__)

SOURCES_LIST = get_importlib_files('satellite_tle').joinpath('sources.csv')

REQUESTS_TIMEOUT = 20  # seconds


def get_tle_sources():
    '''
    Returns a list of (source, url)-tuples for well-known TLE sources.
    '''

    sources = []

    with SOURCES_LIST.open(newline='') as csvfile:
        csv_reader = csv.reader(csvfile,
                                delimiter=',',
                                quotechar='\'',
                                quoting=csv.QUOTE_NONNUMERIC)
        for row in csv_reader:
            source, url = row
            sources.append((source, url))

    return sources


def fetch_tle_from_celestrak(norad_cat_id, verify=True):
    '''
    Returns the TLE for a given norad_cat_id as currently available from CelesTrak.
    Raises IndexError if no data is available for the given norad_cat_id.

    Parameters
    ----------
    norad_cat_id : string
        Satellite Catalog Number (5-digit)
    verify : boolean or string, optional
        Either a boolean, in which case it controls whether we verify
        the server's TLS certificate, or a string, in which case it must be a path
        to a CA bundle to use. Defaults to ``True``. (from python-requests)
    '''

    r = requests.get('https://celestrak.org/NORAD/elements/gp.php?CATNR={}'.format(norad_cat_id),
                     verify=verify,
                     timeout=REQUESTS_TIMEOUT)
    r.raise_for_status()

    if r.text == 'No GP data found':
        raise LookupError

    tle = r.text.split('\r\n')

    return tle[0].strip(), tle[1].strip(), tle[2].strip()


def parse_TLE_file(content):
    '''
    Parses TLE file with 3le format.
    Returns a dictionary of the form {norad_id1: tle1, norad_id2: tle2} for all TLEs found.
    tleN is returned as list of three strings: [satellite_name, line1, line2].
    '''
    tles = dict()
    lines = content.strip().splitlines()

    if len(lines) % 3 != 0:
        raise ValueError

    # Loop over TLEs
    for i in range(len(lines) - 2):
        if (lines[i + 1][0] == "1") & (lines[i + 2][0] == "2"):
            try:
                twoline2rv(lines[i + 1], lines[i + 2], wgs72)
                norad_cat_id = int(lines[i + 1][2:7].encode('ascii'))
                tles[norad_cat_id] = (lines[i].strip(), lines[i + 1], lines[i + 2])
            except ValueError:
                logging.warning('Failed to parse TLE for {}\n({}, {})'.format(
                    lines[i], lines[i + 1], lines[i + 2]))
                continue

    return tles


def fetch_tles_from_spacetrack(spacetrack_config):
    '''
    Downloads the TLE set from Space-Track.org.
    Returns a dictionary of the form {norad_id1: tle1, norad_id2: tle2, ...} for all TLEs found.
    tleN is returned as list of three strings: [satellite_name, line1, line2].

    Parameters
    ----------
    norad_ids : set of integers
        Set of Satellite Catalog Numbers (5-digit)
    spacetrack_config : dictionary
        Credentials for log in Space-Track.org following this format:
        {'identity': <username>, 'password': <password>}
    '''
    with SpaceTrackClient(spacetrack_config['identity'], spacetrack_config['password']) as st:
        tles_3le = st.gp(epoch='>now-30', orderby=["norad_cat_id"], format='3le')

    try:
        return parse_TLE_file(tles_3le)
    except ValueError:
        logging.error('TLE source is malformed.')
        raise ValueError


def zip_extract_all(file):
    '''
    Returns the extracted content of all files in the zip file concatenated into a single string.

    Parameters
    ----------
    file : str or BytesIO
        Path to a valid zip file (a string), or file-like object containing a zip file
    '''
    with StringIO() as out_buffer:
        with ZipFile(file) as zip_file:
            # process all files in the zip file.
            for filename in zip_file.namelist():
                # extract each file to memory and fix the Windows style line endings.
                out_buffer.write(zip_file.read(filename).decode().replace('\r\n', '\n'))
        return out_buffer.getvalue()


def _get_content_from_uri(url, verify=True):
    '''
    Returns the content of the resource at the given URL.

    If resource is a zipfile, returns the concatenated content of all extracted files.

    Example of supported URIs:
    - https://example.com/to/tlefile.txt
    - https://example.com/to/tlefiles.zip
    - file:///path/to/local/tlefile.txt

    Parameters
    ----------
    uri: str
        URI of the resource. Suported protocol schemes: file (localhost only), http, https
    verify : boolean or string, optional
        transparently passed to `requests.get`

    Returns
    -------
    content: str
    '''
    uri = urlparse(url)

    if uri.scheme in ('http', 'https'):
        # Download from the internet
        r = requests.get(url, verify=verify, timeout=REQUESTS_TIMEOUT)
        r.raise_for_status()

        with BytesIO(r.content) as buffer:
            is_zip = is_zipfile(buffer)
            buffer.seek(0)

            if is_zip:
                return zip_extract_all(BytesIO(r.content))
            else:
                return r.text
    elif uri.scheme == 'file':
        # Load from local file
        path = unquote(uri.path)
        if is_zipfile(path):
            return zip_extract_all(path)
        else:
            with open(path, 'rt') as tle_file:
                return tle_file.read()
    logging.error('Unsupported protocol scheme %s for TLE source %s', uri.scheme, uri)
    raise ValueError


def fetch_tles_from_url(url, verify=True):
    '''
    Downloads the TLE set from the given url.
    Returns a dictionary of the form {norad_id1: tle1, norad_id2: tle2} for all TLEs found.
    tleN is returned as list of three strings: [satellite_name, line1, line2].

    Parameters
    ----------
    url : string
        URI of the TLE source. Supported protocols: file (localhost only), http, https
    verify : boolean or string, optional
        Either a boolean, in which case it controls whether we verify
        the server's TLS certificate, or a string, in which case it must be a path
        to a CA bundle to use. Defaults to ``True``. (from python-requests)
    '''
    content = _get_content_from_uri(url, verify)

    try:
        return parse_TLE_file(content)
    except ValueError:
        logging.error('TLE source {} is malformed.'.format(url))
        raise ValueError
