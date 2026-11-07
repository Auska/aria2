/* <!-- copyright */
/*
 * aria2 - The high speed download utility
 *
 * Copyright (C) 2006 Tatsuhiro Tsujikawa
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 * In addition, as a special exception, the copyright holders give
 * permission to link the code of portions of this program with the
 * OpenSSL library under certain conditions as described in each
 * individual source file, and distribute linked combinations
 * including the two.
 * You must obey the GNU General Public License in all respects
 * for all of the code used other than OpenSSL.  If you modify
 * file(s) with this exception, you may extend this exception to your
 * version of the file(s), but you are not obligated to do so.  If you
 * do not wish to do so, delete this exception statement from your
 * version.  If you delete this exception statement from all source
 * files in the program, then also delete it here.
 */
/* copyright --> */
#ifndef D_MESSAGE_H
#define D_MESSAGE_H

#include "common.h"

// clang-format off

#define MSG_SEGMENT_DOWNLOAD_COMPLETED                                  \
  "CUID#{} - The download for one segment completed successfully."
#define MSG_NO_SEGMENT_AVAILABLE "CUID#{} - No segment available."
#define MSG_CONNECTING_TO_SERVER "CUID#{} - Connecting to {}:{}"
#define MSG_REDIRECT "CUID#{} - Redirecting to {}"
#define MSG_SENDING_REQUEST "CUID#{} - Requesting:\n{}"
#define MSG_RECEIVE_RESPONSE "CUID#{} - Response received:\n{}"
#define MSG_DOWNLOAD_ABORTED "CUID#{} - Download aborted. URI={}"
#define MSG_RESTARTING_DOWNLOAD "CUID#{} - Restarting the download. URI={}"
#define MSG_TORRENT_DOWNLOAD_ABORTED "CUID#{} - Download aborted."
#define MSG_MAX_TRY                                                     \
  "CUID#{} - {} times attempted, but no success. Download aborted."
#define MSG_SEND_PEER_MESSAGE "CUID#{} - To: {}:{} {}"
#define MSG_RECEIVE_PEER_MESSAGE "CUID#{} - From: {}:{} {}"
#define MSG_GOT_NEW_PIECE "CUID#{} - we got new piece. index={}"
#define MSG_GOT_WRONG_PIECE "CUID#{} - we got wrong piece. index={}"
#define MSG_DOWNLOAD_NOT_COMPLETE "CUID#{} - Download not complete: {}"
#define MSG_DOWNLOAD_ALREADY_COMPLETED _("GID#{} - Download has already completed: {}")
#define MSG_RESOLVING_HOSTNAME "CUID#{} - Resolving hostname {}"
#define MSG_NAME_RESOLUTION_COMPLETE                    \
  "CUID#{} - Name resolution complete: {} -> {}"
#define MSG_NAME_RESOLUTION_FAILED                      \
  "CUID#{} - Name resolution for {} failed:{}"
#define MSG_DNS_CACHE_HIT "CUID#{} - DNS cache hit: {} -> {}"
#define MSG_CONNECTING_TO_PEER "CUID#{} - Connecting to the peer {}"
#define MSG_PIECE_RECEIVED                                              \
  "CUID#{} - Piece received. index={}, begin={}, length={}, offset={}," \
  " blockIndex={}"
#define MSG_PIECE_BITFIELD "CUID#{} - Piece bitfield {}"
#define MSG_REJECT_PIECE_CHOKED                                         \
  "CUID#{} - Reject piece message in queue because the peer has been" \
  " choked. index={}, begin={}, length={}"
#define MSG_REJECT_PIECE_CANCEL                                         \
  "CUID#{} - Reject piece message in queue because cancel message received." \
  " index={}, begin={}, length={}"
#define MSG_FILE_VALIDATION_FAILURE                             \
  "CUID#{} - Exception caught while validating file integrity."
#define MSG_PEER_INTERESTED "CUID#{} - Interested in the peer"
#define MSG_PEER_NOT_INTERESTED "CUID#{} - Not interested in the peer"
#define MSG_DELETING_REQUEST_SLOT "CUID#{} - Deleting request slot" \
  " index={}, begin={}, blockIndex={}"
#define MSG_DELETING_REQUEST_SLOT_CHOKED "CUID#{} - Deleting request slot" \
  " index={}, begin={}, blockIndex={} because localhost got choked."
#define MSG_DELETING_REQUEST_SLOT_TIMEOUT "CUID#{} - Deleting request slot" \
  " index={}, begin={}, blockIndex={} because of time out"
#define MSG_DELETING_REQUEST_SLOT_ACQUIRED "CUID#{} - Deleting request slot" \
  " index={}, begin={}, blockIndex={} because the block has been acquired."
#define MSG_FAST_EXTENSION_ENABLED "CUID#{} - Fast extension enabled."
#define MSG_EXTENDED_MESSAGING_ENABLED "CUID#{} - Extended Messaging enabled."
#define MSG_FILE_ALLOCATION_FAILURE                             \
  "CUID#{} - Exception caught while allocating file space."
#define MSG_CONTENT_DISPOSITION_DETECTED                        \
  "CUID#{} - Content-Disposition detected. Use {} as filename"
#define MSG_PEER_BANNED "CUID#{} - Peer {}:{} banned."
#define MSG_LISTENING_PORT                                      \
  "CUID#{} - Using port {} for accepting new connections"
#define MSG_BIND_FAILURE "CUID#{} - An error occurred while binding port={}"
#define MSG_INCOMING_PEER_CONNECTION                            \
  "CUID#{} - Incoming connection, adding new command CUID#{}"
#define MSG_ACCEPT_FAILURE "CUID#{} - Error in accepting connection"
#define MSG_TRACKER_RESPONSE_PROCESSING_FAILED                  \
  "CUID#{} - Error occurred while processing tracker response."
#define MSG_DHT_ENABLED_PEER "CUID#{} - The peer is DHT-enabled."
#define MSG_CONNECT_FAILED_AND_RETRY            \
  "CUID#{} - Could not to connect to {}:{}. Trying another address"

#define MSG_UNRECOGNIZED_URI _("Unrecognized URI or unsupported protocol: {}")
#define MSG_TRACKER_WARNING_MESSAGE _("Tracker returned warning message: {}")
#define MSG_SEGMENT_FILE_EXISTS _("The segment file {} exists.")
#define MSG_SEGMENT_FILE_DOES_NOT_EXIST _("The segment file {} does not exist.")
#define MSG_SAVING_SEGMENT_FILE _("Saving the segment file {}")
#define MSG_SAVED_SEGMENT_FILE _("The segment file was saved successfully.")
#define MSG_LOADING_SEGMENT_FILE _("Loading the segment file {}.")
#define MSG_LOADED_SEGMENT_FILE _("The segment file was loaded successfully.")
#define MSG_NO_URL_TO_DOWNLOAD _("No URI to download. Download aborted.")
#define MSG_FILE_ALREADY_EXISTS _("File {} exists, but a control file(*.aria2) does not exist. Download was canceled in order to prevent your file from being truncated to 0. If you are sure to download the file all over again, then delete it or add --allow-overwrite=true option and restart aria2.")
#define MSG_ALLOCATING_FILE _("Allocating file {}, {} bytes")
#define MSG_FILE_NOT_FOUND _("File not found")
#define MSG_NOT_DIRECTORY _("Not a directory")
#define MSG_INSUFFICIENT_CHECKSUM _("Insufficient checksums. checksumLength={}, numChecksum={}")
#define MSG_WRITING_FILE _("Writing file {}")
#define MSG_NO_PEER_LIST_RECEIVED _("No peer list received.")
#define MSG_ADDING_PEER _("Adding peer {}:{}")
#define MSG_DELETING_USED_PIECE _("Deleting used piece index={}, fillRate(%)={}<={}")
#define MSG_SELECTIVE_DOWNLOAD_COMPLETED _("Download of selected files was complete.")
#define MSG_DOWNLOAD_COMPLETED _("The download was complete.")
#define MSG_REMOVED_HAVE_ENTRY _("Removed {} have entries.")
#define MSG_VALIDATING_FILE _("Validating file {}")
#define MSG_ALLOCATION_COMPLETED "{} seconds to allocate {} byte(s)"
#define MSG_FILE_ALLOCATION_DISPATCH                    \
  "Dispatching FileAllocationCommand for CUID#{}."
#define MSG_FILE_DOWNLOAD_COMPLETED _("Download complete: {}")
#define MSG_SEEDING_END _("Seeding is over.")
#define MSG_NO_CHUNK_CHECKSUM _("No chunk to verify.")
#define MSG_GOOD_CHUNK_CHECKSUM _("Good chunk checksum. hash={}")
#define MSG_LOADING_COOKIE_FAILED _("Failed to load cookies from {}")
#define MSG_INCORRECT_NETRC_PERMISSION _(".netrc file {} does not have correct permissions. It should be 600. netrc support disabled.")
#define MSG_LOGGING_STARTED _("Logging started.")
#define MSG_URI_REQUIRED _("Specify at least one URL.")
#define MSG_DAEMON_FAILED _("daemon failed.")
#define MSG_VERIFICATION_SUCCESSFUL _("Verification finished successfully. file={}")
#define MSG_VERIFICATION_FAILED _("Checksum error detected. file={}")
#define MSG_INCOMPLETE_RANGE _("Incomplete range specified. {}")
#define MSG_STRING_INTEGER_CONVERSION_FAILURE _("Failed to convert string into value: {}")
#define MSG_RESOURCE_NOT_FOUND _("Resource not found")
#define MSG_FILE_RENAMED _("File already exists. Renamed to {}.")
#define MSG_TOO_SMALL_PAYLOAD_SIZE _("Too small payload size for {}, size={}.")
#define MSG_REMOVED_DEFUNCT_CONTROL_FILE _("Removed the defunct control file {} because the download file {} doesn't exist.")
#define MSG_SHARE_RATIO_REPORT _("Your share ratio was {:.1f}, uploaded/downloaded={}B/{}B")
#define MSG_MISSING_BT_INFO _("Missing {} in torrent metainfo.")
#define MSG_NEGATIVE_LENGTH_BT_INFO _("{} does not allow negative integer {}")
#define MSG_NULL_TRACKER_RESPONSE _("Tracker returned null data.")
#define MSG_TIME_HAS_PASSED _("{} second(s) has passed. Stopping application.")
#define MSG_SIGNATURE_SAVED _("Saved signature as {}. Please note that aria2" \
                              " doesn't verify signatures.")
#define MSG_SIGNATURE_NOT_SAVED _("Saving signature as {} failed. Maybe file" \
                                  " already exists.")
#define MSG_OPENING_READABLE_SERVER_STAT_FILE_FAILED    \
  _("Failed to open ServerStat file {} for read.")
#define MSG_SERVER_STAT_LOADED _("ServerStat file {} loaded successfully.")
#define MSG_READING_SERVER_STAT_FILE_FAILED _("Failed to read ServerStat from" \
                                              " {}.")
#define MSG_OPENING_WRITABLE_SERVER_STAT_FILE_FAILED    \
  _("Failed to open ServerStat file {} for write.")
#define MSG_SERVER_STAT_SAVED _("ServerStat file {} saved successfully.")
#define MSG_WRITING_SERVER_STAT_FILE_FAILED _("Failed to write ServerStat to" \
                                              " {}.")
#define MSG_ESTABLISHING_CONNECTION_FAILED              \
  _("Failed to establish connection, cause: {}")
#define MSG_NETWORK_PROBLEM _("Network problem has occurred. cause:{}")
#define MSG_LOADING_SYSTEM_TRUSTED_CA_CERTS_FAILED                              \
  _("Failed to load trusted CA certificates from system. Cause: {}")
#define MSG_LOADING_TRUSTED_CA_CERT_FAILED                              \
  _("Failed to load trusted CA certificates from {}. Cause: {}")
#define MSG_CERT_VERIFICATION_FAILED                    \
  _("Certificate verification failed. Cause: {} See --ca-certificate and" \
    " --check-certificate option.")
#define MSG_NO_CERT_FOUND _("No certificate found.")
#define MSG_HOSTNAME_NOT_MATCH _("Hostname not match.")
#define MSG_NO_FILES_TO_DOWNLOAD _("No files to download.")
#define MSG_WARN_NO_CA_CERT                                             \
  _("You may encounter the certificate verification error with HTTPS server." \
    " See --ca-certificate and --check-certificate option.")
#define MSG_WARN_UNKNOWN_TLS_CONNECTION \
  _("aria2c had to connect to the other side using an unknown TLS protocol. " \
    "The integrity and confidentiality of the connection might be " \
    "compromised.\nPeer: {}")
#define MSG_WARN_OLD_TLS_CONNECTION \
  _("aria2c had to connect to the other side using an old and vulnerable TLS" \
    " protocol. The integrity and confidentiality of the connection might be" \
    " compromised.\nProtocol: {}, Peer: {}")
#define MSG_SHOW_FILES _("Printing the contents of file '{}'...")
#define MSG_NOT_TORRENT _("This file is not a Torrent file. Skipping.")
#define MSG_GID_NOT_PROVIDED "GID is not provided."
#define MSG_GOOD_BYE_SEEDER "Client is in seed state: Good Bye Seeder;)"
#define MSG_NOT_FILE _("Is '{}' a file?")
#define MSG_INTERFACE_NOT_FOUND _("Failed to find given interface {},"  \
                                  " cause: {}")
#define MSG_METADATA_SAVED _("Saved metadata as {}.")
#define MSG_METADATA_NOT_SAVED _("Saving metadata as {} failed. Maybe file" \
                                 " already exists.")
#define MSG_DIR_TRAVERSAL_DETECTED _("Detected directory traversal directive in {}")
#define MSG_HASH_CHECK_NOT_DONE                                         \
  "File has already been downloaded but hash check has not been done yet."
#define MSG_REMOVING_UNSELECTED_FILE _("GID#{} - Removing unselected file.")
#define MSG_FILE_REMOVED _("File {} removed.")
#define MSG_FILE_COULD_NOT_REMOVED _("File {} could not be removed.")

#define EX_TIME_OUT _("Timeout.")
#define EX_INVALID_CHUNK_SIZE _("Invalid chunk size.")
#define EX_TOO_LARGE_CHUNK _("Too large chunk. size={}")
#define EX_INVALID_HEADER _("Invalid header.")
#define EX_INVALID_RESPONSE _("Invalid response.")
#define EX_NO_HEADER _("No header found.")
#define EX_NO_STATUS_HEADER _("No status header.")
#define EX_PROXY_CONNECTION_FAILED _("Proxy connection failed.")
#define EX_CONNECTION_FAILED _("Connection failed.")
#define EX_FILENAME_MISMATCH _("The requested filename and the previously registered one are not same. Expected:{} Actual:{}")
#define EX_BAD_STATUS _("The response status is not successful. status={}")
#define EX_TOO_LARGE_FILE "Too large file size. size={}"
#define EX_TRANSFER_ENCODING_NOT_SUPPORTED _("Transfer encoding {} is not supported.")
#define EX_SSL_INIT_FAILURE _("SSL initialization failed: {}")
#define EX_SSL_IO_ERROR _("SSL I/O error")
#define EX_SSL_PROTOCOL_ERROR _("SSL protocol error")
#define EX_SSL_UNKNOWN_ERROR _("SSL unknown error {}")
#define EX_SSL_CONNECT_ERROR _("SSL initialization failed: OpenSSL connect error {}")
#define EX_SIZE_MISMATCH "Size mismatch Expected:{} Actual:{}"
#define EX_AUTH_FAILED _("Authorization failed.")
#define EX_GOT_EOF _("Got EOF from the server.")
#define EX_EOF_FROM_PEER _("Got EOF from peer.")
#define EX_MALFORMED_META_INFO _("Malformed meta info.")

#define EX_FILE_OPEN _("Failed to open the file {}, cause: {}")
#define EX_FILE_WRITE _("Failed to write into the file {}, cause: {}")
#define EX_FILE_READ _("Failed to read from the file {}, cause: {}")
#define EX_DATA_READ _("Failed to read data from disk.")
#define EX_FILE_SHA1SUM _("Failed to calculate SHA1 digest of or a part of the file {}, cause: {}")
#define EX_FILE_SEEK _("Failed to seek the file {}, cause: {}")
#define EX_FILE_OFFSET_OUT_OF_RANGE "The offset is out of range, offset={}"
#define EX_NOT_DIRECTORY _("{} is not a directory.")
#define EX_MAKE_DIR _("Failed to make the directory {}, cause: {}")
#define EX_SEGMENT_FILE_WRITE "Failed to write into the segment file {}"
#define EX_SEGMENT_FILE_READ "Failed to read from the segment file {}"

#define EX_SOCKET_OPEN _("Failed to open a socket, cause: {}")
#define EX_SOCKET_SET_OPT _("Failed to set a socket option, cause: {}")
#define EX_SOCKET_BLOCKING _("Failed to set a socket as blocking, cause: {}")
#define EX_SOCKET_NONBLOCKING _("Failed to set a socket as non-blocking, cause: {}")
#define EX_SOCKET_BIND _("Failed to bind a socket, cause: {}")
#define EX_SOCKET_LISTEN _("Failed to listen to a socket, cause: {}")
#define EX_SOCKET_ACCEPT _("Failed to accept a peer connection, cause: {}")
#define EX_SOCKET_GET_NAME _("Failed to get the name of socket, cause: {}")
#define EX_SOCKET_GET_PEER _("Failed to get the name of connected peer, cause: {}")
#define EX_RESOLVE_HOSTNAME _("Failed to resolve the hostname {}, cause: {}")
#define EX_SOCKET_CONNECT _("Failed to connect to the host {}, cause: {}")
#define EX_SOCKET_CHECK_WRITABLE _("Failed to check whether the socket is writable, cause: {}")
#define EX_SOCKET_CHECK_READABLE _("Failed to check whether the socket is readable, cause: {}")
#define EX_SOCKET_SEND _("Failed to send data, cause: {}")
#define EX_SOCKET_RECV _("Failed to receive data, cause: {}")
#define EX_SOCKET_PEEK _("Failed to peek data, cause: {}")
#define EX_SOCKET_UNKNOWN_ERROR _("Unknown socket error {} (0x{})")
#define EX_FILE_ALREADY_EXISTS _("File {} exists, but {} does not exist.")
#define EX_INVALID_PAYLOAD_SIZE                                 \
  _("Invalid payload size for {}, size={}. It should be {}.")
#define EX_INVALID_BT_MESSAGE_ID _("Invalid ID={} for {}. It should be {}.")
#define EX_INVALID_CHUNK_CHECKSUM "Chunk checksum validation failed. checksumIndex={}, offset={}, expectedHash={}, actualHash={}"
#define EX_DOWNLOAD_ABORTED _("Download aborted.")
#define EX_DUPLICATE_FILE_DOWNLOAD _("File {} is being downloaded by other command.")
#define EX_INSUFFICIENT_CHECKSUM _("Insufficient checksums.")
#define EX_TRACKER_FAILURE _("Tracker returned failure reason: {}")
#define EX_FLOODING_DETECTED _("Flooding detected.")
#define EX_DROP_INACTIVE_CONNECTION \
  _("Drop connection because no request/piece messages were exchanged in a" \
    " certain period({} seconds).")
#define EX_INFOHASH_MISMATCH_IN_SEGFILE _("The infoHash in torrent file doesn't match to one in .aria2 file.")
#define EX_NO_SUCH_FILE_ENTRY _("No such file entry {}")
#define EX_TOO_SLOW_DOWNLOAD_SPEED _("Too slow Downloading speed: {} <= {}(B/s), host:{}")
#define EX_NO_HTTP_REQUEST_ENTRY_FOUND _("No HttpRequestEntry found.")
#define EX_LOCATION_HEADER_REQUIRED _("Got {} status, but no location header provided.")
#define EX_INVALID_RANGE_HEADER "Invalid range header. Request: {}-{}/{}, Response: {}-{}/{}"
#define EX_NO_RESULT_WITH_YOUR_PREFS _("No file matched with your preference.")
#define EX_EXCEPTION_CAUGHT _("Exception caught")
#define EX_TOO_LONG_PAYLOAD _("Max payload length exceeded or invalid. length = {}")
#define EX_FILE_LENGTH_MISMATCH_BETWEEN_LOCAL_AND_REMOTE _("Invalid file length. Cannot continue download {}: local {}, remote {}")

// clang-format on

#endif // D_MESSAGE_H
