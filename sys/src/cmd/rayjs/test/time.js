// The runner supplies a zone with one daylight-saving interval in 2020.
const time = 1595000000000;
for (const [stamp, offset] of [[time, -120], [time + 2**32*1000, -60], [time - 2**32*1000, -60]]) {
    if (new Date(stamp).getTimezoneOffset() !== offset) throw Error('time zone lost high bits');
}
print('rayjs: time passed');
