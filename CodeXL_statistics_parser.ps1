$statistic = Get-Content c:\Users\Павел\AppData\Local\Temp\AMD\CodeXL\xrLC_KernelOutput\bvh_cl\LightingPoints\Statistics.cxltxt
$csv_table = ConvertFrom-Csv $statistic -Delimiter ';'
$csv_table