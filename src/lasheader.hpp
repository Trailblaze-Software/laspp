
#pragma once

#include <cstdint>
#include <iostream>
#include <sstream>
#include <fstream>

namespace laspp {

#pragma pack(push, 1)
  enum GlobalEncoding : uint16_t {
    GPS_TIME = 2<<0,
    WAVEFORM_DATA_INTERNAL = 2<<1,
    WAVEFORM_DATA_EXTERNAL = 2<<2,
    SYNTHETIC_RETURN_NUMBERS = 2<<3,
    WKT = 2<<4,
  };

  inline std::string global_encoding_string(const uint16_t& encoding) {
    std::stringstream ss;
    if (encoding & GPS_TIME) ss << "GPS time, ";
    if (encoding & WAVEFORM_DATA_INTERNAL) ss << "Waveform data internal, ";
    if (encoding & WAVEFORM_DATA_EXTERNAL) ss << "Waveform data external, ";
    if (encoding & SYNTHETIC_RETURN_NUMBERS) ss << "Synthetic return numbers, ";
    if (encoding & WKT) ss << "WKT, ";
    return ss.str();
  }

  template <typename T, size_t N>
  std::string arr_to_string(const T (&arr)[N]) {
    std::stringstream ss;
    ss << "[";
    for (size_t i = 0; i < N; ++i) {
      ss << arr[i];
      if (i < N - 1) ss << ", ";
    }
    ss << "]";
    return ss.str();
  }

  struct __attribute__ ((packed)) LASHeader {
    char file_signature[4] = {'L', 'A', 'S', 'F'};
    uint16_t file_source_id;
    uint16_t global_encoding;
    uint32_t project_id_1;
    uint16_t project_id_2;
    uint16_t project_id_3;
    uint8_t project_id_4[8];
    uint8_t version_major;
    uint8_t version_minor;
    char system_id[32];
    char generating_software[32];
    uint16_t file_creation_day;
    uint16_t file_creation_year;
    uint16_t header_size;
    uint32_t offset_to_point_data;
    uint32_t number_of_variable_length_records;
    uint8_t point_data_record_format;
    uint16_t point_data_record_length;
    uint32_t legacy_number_of_point_records;
    uint32_t legacy_number_of_points_by_return[5];
    double x_scale_factor;
    double y_scale_factor;
    double z_scale_factor;
    double x_offset;
    double y_offset;
    double z_offset;
    double max_x;
    double max_y;
    double max_z;
    double min_x;
    double min_y;
    double min_z;
    uint64_t start_of_waveform_data_packet_record;
    uint64_t start_of_first_extended_variable_length_record;
    uint32_t number_of_extended_variable_length_records;
    uint64_t number_of_point_records;
    uint64_t number_of_points_by_return[15];

    static LASHeader FromFile(std::ifstream& file) {
      LASHeader header;
      file.read(reinterpret_cast<char*>(&header), sizeof(LASHeader));
      return header;
    }

    friend std::ostream& operator<<(std::ostream& os, const LASHeader& header) {
      os << "File signature: " << header.file_signature[0] << header.file_signature[1] << header.file_signature[2] << header.file_signature[3] << std::endl;
      os << "File source ID: " << header.file_source_id << std::endl;
      os << "Global encoding: " << global_encoding_string(header.global_encoding) << std::endl;
      os << "Project ID 1: " << header.project_id_1 << std::endl;
      os << "Project ID 2: " << header.project_id_2 << std::endl;
      os << "Project ID 3: " << header.project_id_3 << std::endl;
      os << "Project ID 4: " << arr_to_string(header.project_id_4) << std::endl;
      os << "Version major: " << header.version_major << std::endl;
      os << "Version minor: " << header.version_minor << std::endl;
      os << "System ID: " << header.system_id << std::endl;
      os << "Generating software: " << header.generating_software << std::endl;
      os << "File creation day: " << header.file_creation_day << std::endl;
      os << "File creation year: " << header.file_creation_year << std::endl;
      os << "Header size: " << header.header_size << std::endl;
      os << "Offset to point data: " << header.offset_to_point_data << std::endl;
      os << "Number of variable length records: " << header.number_of_variable_length_records << std::endl;
      os << "Point data record format: " << (uint)header.point_data_record_format << std::endl;
      os << "Point data record length: " << header.point_data_record_length << std::endl;
      os << "Legacy number of point records: " << header.legacy_number_of_point_records << std::endl;
      os << "Legacy number of points by return: " << arr_to_string(header.legacy_number_of_points_by_return) << std::endl;
      os << "X scale factor: " << header.x_scale_factor << std::endl;
      os << "Y scale factor: " << header.y_scale_factor << std::endl;
      os << "Z scale factor: " << header.z_scale_factor << std::endl;
      os << "X offset: " << header.x_offset << std::endl;
      os << "Y offset: " << header.y_offset << std::endl;
      os << "Z offset: " << header.z_offset << std::endl;
      os << "Max X: " << header.max_x << std::endl;
      os << "Max Y: " << header.max_y << std::endl;
      os << "Max Z: " << header.max_z << std::endl;
      os << "Min X: " << header.min_x << std::endl;
      os << "Min Y: " << header.min_y << std::endl;
      os << "Min Z: " << header.min_z << std::endl;
      os << "Start of waveform data packet record: " << header.start_of_waveform_data_packet_record << std::endl;
      os << "Start of first extended variable length record: " << header.start_of_first_extended_variable_length_record << std::endl;
      os << "Number of extended variable length records: " << header.number_of_extended_variable_length_records << std::endl;
      os << "Number of point records: " << header.number_of_point_records << std::endl;
      os << "Number of points by return: " << arr_to_string(header.number_of_points_by_return) << std::endl;
      return os;
    }
  };

#pragma pack(pop)

}
