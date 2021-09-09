#!/usr/bin/python2.7
# -*- coding: utf-8 -*-

import argparse
import os
import sys
import cv2
import numpy as np
import shutil

LAONSILL_CLIENT_LIB_PATH = os.path.join(os.environ['LAONSILL_HOME'], 'dev/client/lib')
sys.path.append(LAONSILL_CLIENT_LIB_PATH)
from libLaonSillClient import *

import pdb


SDF_TYPE_DATUM = 'DATUM'
SDF_TYPE_ANNO_DATUM = 'ANNO_DATUM'


def get_arguments():
    """Parse all the arguments provided from the CLI.
    
    Returns:
      A list of parsed arguments.
    """
    parser = argparse.ArgumentParser(description="DeepLab-ResNet Network")
    parser.add_argument("--sdf-path", type=str, required=True, 
                        help="")
    parser.add_argument("--dataset-name", type=str, required=False,
                        help="")
    parser.add_argument("--out-path", type=str, required=True,
                        help="")

    return parser.parse_args()


def decodeDatum(sdfPath, dataSet, out_path):
    header = RetrieveSDFHeader(sdfPath)
    sdfType = header.type

    dataReader = None
    if sdfType == SDF_TYPE_DATUM:
        dataReader = DataReaderDatum(sdfPath)
    elif sdfType == SDF_TYPE_ANNO_DATUM: 
        dataReader = DataReaderAnnoDatum(sdfPath)

    header = dataReader.getHeader()
    print(":: HEADER ::")
    header.info()
    print("::::::::::::")

    set_size = header.setSizes[0]
    digits = len(str(set_size))

    labelItemList = header.labelItemList
    labelItemDict = dict()
    for labelItem in labelItemList:
        labelItemDict[labelItem.label] = labelItem


    if dataSet == None:
        dataReader.selectDataSetByIndex(0)
    else:
        dataReader.selectDataSetByName(dataSet)

    key = 0
    for idx in xrange(set_size):
        datum = dataReader.getNextData()

        if datum.encoded:
            im = cv2.imdecode(np.fromstring(datum.data, np.uint8), cv2.CV_LOAD_IMAGE_COLOR)
        else:
            im = np.fromstring(datum.data, np.uint8).reshape(datum.channels, datum.height,
                    datum.width).transpose(1, 2, 0)

            cv2.imwrite(os.path.join(out_path, "{}.jpg".format(str(idx + 1).zfill(digits))), im);



def main():
    """Create the model and start the training."""
    args = get_arguments()

    sdf_path = os.path.expanduser(args.sdf_path)
    dataset_name = args.dataset_name
    out_path = os.path.expanduser(args.out_path)

    assert os.path.exists(sdf_path), "sdf-path not exists: {}".format(sdf_path)
    assert os.path.exists(out_path), "out-path not exists: {}".format(out_path)


    print("press ESC key to quit")
    print("press Any key but ESC to see Next Datum")
    decodeDatum(sdf_path, dataset_name, out_path)
    

if __name__ == '__main__':
    main()



