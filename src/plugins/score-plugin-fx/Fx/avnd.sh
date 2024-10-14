#!/bin/bash
sed -Ei 's/static const constexpr auto prettyName = "(.*)";/halp_meta(name, "\1")/' *.hpp
sed -Ei 's/static const constexpr auto objectKey = "(.*)";/halp_meta(c_name, "\1")/'  *.hpp
sed -Ei 's/static const constexpr auto category = "(.*)";/halp_meta(category, "\1")/'  *.hpp
sed -Ei 's/static const constexpr auto author = "(.*)";/halp_meta(author, "\1")/'  *.hpp
sed -Ei 's/static const constexpr auto manual_url = "(.*)";/halp_meta(manual_url, "\1")/'  *.hpp
sed -Ei 's/static const constexpr auto description = "(.*)";/halp_meta(description, "\1")/'  *.hpp
sed -Ei 's/static const constexpr auto uuid = make_uuid\("(.*)"\);/halp_meta(uuid, "\1")/'  *.hpp
sed -i '/static const constexpr auto kind.*/d'  *.hpp
sed -i '/static const constexpr auto tags.*/d'  *.hpp
