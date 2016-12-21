#!/usr/bin/python3
import os
import sys
import fnmatch
import xml.dom.minidom

def isEmptyNode(node):
    return node.nodeType == node.TEXT_NODE and node.data.strip() == ''

def removeEmptyNodes(parent):
    if parent.childNodes:
       for child in list(parent.childNodes):
            if isEmptyNode(child):
                parent.removeChild(child)
                continue
            removeEmptyNodes(child)

#annotations with no value crash gdbus-codegen
def fixInvalidAttrs(parent):
   for child in parent.getElementsByTagName("annotation"):
        if not child.hasAttribute("value"):
            child.setAttribute("value", "")

seenInterfaces = set()

def scanFile(filename, destDir):
    print("Reading {0}".format(filename))
    with open(filename, "rb") as f:
        filedata = f.read()
        curPos = None
        nextStart = -1
        while True:
            nextStart = filedata.find(b"<interface ", curPos)
            if nextStart < 0:
                break
            endTag = b"</interface>"
            curPos = filedata.find(endTag, nextStart) + len(endTag)
            xmlStr = filedata[nextStart:curPos].decode("utf-8")
            try:
                xmlData = xml.dom.minidom.parseString(xmlStr)
                ifaceName = xmlData.documentElement.getAttribute("name")
                if ifaceName not in seenInterfaces:
                    print("Found {0}".format(ifaceName))
                    seenInterfaces.add(ifaceName)
                    xmlName = os.path.join(destDir, "{0}.xml".format(ifaceName))
                    removeEmptyNodes(xmlData)
                    fixInvalidAttrs(xmlData)
                    newNode = xmlData.createElement("node")
                    newNode.appendChild(xmlData.documentElement)
                    prettyString = newNode.toprettyxml();
                    with open(xmlName, "w") as o:
                        o.write(prettyString)
            except Exception as e:
                print(e)
                print(xmlStr)

startDir = sys.argv[1]
destDir = sys.argv[2]
print(startDir)
for root, dirnames, filenames in os.walk(startDir):
    for filename in fnmatch.filter(filenames, '*.so'):
        scanFile(os.path.join(root, filename), destDir)



                
    

