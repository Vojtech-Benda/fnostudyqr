# fnostudyqr
A DICOM study query-retriever for querying and downloading DICOM studies from PACS to local computer.
It requires a text file (.txt, .csv) containing a list of patients with filter values: PatientName, PatientID, StudyDate, Modality (optional).
Based on dcmtk's findscu and movescu tools, combining them together.

#### Example patient-list.txt
```
Jane Doe,1234,1.2.2012
John Doe,5678,5.12.2020
John Doe,5678,8.9.2024
Joshua Doe,9123,9.5.2023
```
fnostudyqr will query (C-FIND request) for studies with the above values as DICOM tags and retrieve the corresponding study's StudyInstanceUID tag.
Studies are then donwloaded (C-MOVE request) and stored.

## Usage
```
fnostudyqr pacs-ip pacs-port [options]
```
Studies not found are logged into missing-studies-[Y]-[m]-[d]-[H]-[M]-[S].txt.
```
2025-03-14 14:35:26
PatientID, StudyDate
025, 19970330 - NOT FOUND
```

## Requirements
* fmt v11.1 or newer
* dcmtk v3.6.8 or newer
