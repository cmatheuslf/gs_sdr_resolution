import requests
import logging

from sgp4.earth_gravity import wgs72
from sgp4.io import twoline2rv

from . import (get_tle_sources, fetch_tles_from_url, fetch_tle_from_celestrak,
               fetch_tles_from_spacetrack)


def fetch_all_tles(requested_norad_ids, sources=get_tle_sources(), spacetrack_config=None,
                   verify=True):
    '''
    Returns all TLEs found for the requested satellites available via
    custom TLE sources or via the default sources, Celestrak, CalPoly
    and AMSAT.
    '''
    return fetch_tles(requested_norad_ids, sources, spacetrack_config, verify, only_latest=False)


def fetch_latest_tles(requested_norad_ids, sources=get_tle_sources(), spacetrack_config=None,
                      verify=True):
    '''
    Returns the most recent TLEs found for the requested satellites
    available via custom TLE sources or via the default sources,
    Celestrak, CalPoly and AMSAT.
    '''
    return fetch_tles(requested_norad_ids, sources, spacetrack_config, verify)


def fetch_tles(requested_norad_ids, sources=get_tle_sources(), spacetrack_config=None,
               verify=True, only_latest=True):
    '''
    Returns the most recent or all TLEs found for the requested
    satellites available via custom TLE sources or via the default
    sources, Celestrak, CalPoly and AMSAT.
    '''

    # Dictionary of NORAD IDs with each has as value either a list of
    # 2-tuples of the form (source, tle) or just one 2-tuples
    # depending on only_latest parameter
    # source is a human-readable string
    # tle is a 3-tuple of strings
    tles = dict()

    def update_tles(norad_id, source, tle):
        if norad_id not in requested_norad_ids:
            # Satellite not requested,
            # skip.
            return

        tle_tuple = source, tle

        if norad_id not in tles.keys():
            # Satellite requested and first occurence in the downloaded data,
            # store new TLE.
            if only_latest:
                tles[norad_id] = tle_tuple
            else:
                tles[norad_id] = [tle_tuple]
            return

        if only_latest:
            # There are multiple TLEs for this satellite available.
            # Parse and compare epoch of both TLEs and choose the most recent one.
            current_sat = twoline2rv(tles[norad_id][1][1], tles[norad_id][1][2], wgs72)
            new_sat = twoline2rv(tle[1], tle[2], wgs72)
            if new_sat.epoch > current_sat.epoch:
                # Found a more recent TLE than the current one,
                # store the new TLE.
                logging.debug('Updated {}, epoch '
                              '{:%Y-%m-%d %H:%M:%S} > {:%Y-%m-%d %H:%M:%S}'.format(
                                  norad_id,
                                  new_sat.epoch,
                                  current_sat.epoch))
                tles[norad_id] = tle_tuple
        else:
            tles[norad_id].append(tle_tuple)

    for source, url in sources:
        logging.info('Fetch from {}'.format(url))
        try:
            new_tles = fetch_tles_from_url(url=url, verify=verify)
            logging.debug('Found TLEs for {}'.format(list(new_tles.keys())))
        except (requests.HTTPError, requests.Timeout, requests.ConnectionError):
            logging.warning('Failed to download from {}.'.format(source))
            continue
        except ValueError:
            logging.warning('Failed to parse catalog from {}.'.format(source))
            continue

        for norad_id, tle in new_tles.items():
            update_tles(norad_id, source, tle)

    if spacetrack_config:
        try:
            new_tles = fetch_tles_from_spacetrack(spacetrack_config)
            logging.debug('Found TLEs for {}'.format(list(new_tles.keys())))
            for norad_id, tle in new_tles.items():
                update_tles(norad_id, 'Space-Track.org', tle)
        except (requests.HTTPError, requests.Timeout, requests.ConnectionError):
            logging.warning('Failed to download from Space-Track.org')
        except ValueError:
            logging.warning('Failed to parse catalog from Space-Track.org')

    # Try fetching missing sats from another Celestrak endoint
    missing_norad_ids = set(requested_norad_ids) - set(tles.keys())

    for norad_id in missing_norad_ids:
        try:
            logging.info('Fetch {} from Celestrak (satcat)'.format(norad_id))
            tle = fetch_tle_from_celestrak(norad_id, verify=verify)
            update_tles(norad_id, 'Celestrak (satcat)', tle)
        except (LookupError, requests.HTTPError, requests.Timeout, requests.ConnectionError):
            logging.warning('Fetch {} from Celestrak (satcat) failed!'.format(norad_id))
            continue

    return tles
