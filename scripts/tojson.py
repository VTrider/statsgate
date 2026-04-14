import statsgate_pb2
from google.protobuf import json_format
import sys
import gzip

session = statsgate_pb2.ClientStatSession()

with gzip.open(f"{sys.argv[1]}.binpb.gz", "rb") as f:
    decompressed = f.read()
    session.ParseFromString(decompressed)

with open(f"{sys.argv[1]}.json", "w") as json:
    json.write(json_format.MessageToJson(session, indent=2))
